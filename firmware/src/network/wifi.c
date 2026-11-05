#include "wifi.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "timestamp.h"

static const char *TAG = "WIFI";

#define FAST_RETRY_LIMIT        5
#define INITIAL_BACKOFF_MS      1000
#define MAX_BACKOFF_MS          30000
#define COOLDOWN_BACKOFF_MS     60000

static wifi_status_t s_wifi_status = WIFI_STATUS_UNINITIALIZED;
static int s_retry_count = 0;
static esp_netif_t *s_sta_netif = NULL;
static bool s_netif_initialized = false;
static esp_timer_handle_t s_reconnect_timer = NULL;
static bool s_has_credentials = false;

static void wifi_reconnect_timer_cb(void *arg) {
    if (!s_has_credentials) {
        return;
    }
    if (s_wifi_status == WIFI_STATUS_CONNECTED || s_wifi_status == WIFI_STATUS_STANDALONE) {
        return;
    }

    ESP_LOGI(TAG, "Reconnection timer fired; issuing non-blocking esp_wifi_connect()...");
    s_wifi_status = WIFI_STATUS_CONNECTING;
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect returned: %s", esp_err_to_name(err));
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Wi-Fi station interface started. Connecting to AP...");
            s_wifi_status = WIFI_STATUS_CONNECTING;
            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Initial esp_wifi_connect reported: %s", esp_err_to_name(err));
            }
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (!s_has_credentials) {
                s_wifi_status = WIFI_STATUS_STANDALONE;
                return;
            }

            uint32_t delay_ms;
            if (s_retry_count < FAST_RETRY_LIMIT) {
                delay_ms = INITIAL_BACKOFF_MS * (1 << s_retry_count);
                if (delay_ms > MAX_BACKOFF_MS) {
                    delay_ms = MAX_BACKOFF_MS;
                }
                s_retry_count++;
                s_wifi_status = WIFI_STATUS_DISCONNECTED;
                ESP_LOGW(TAG, "Disconnected from AP. Reconnect attempt %d/%d scheduled in %lu ms...",
                         s_retry_count, FAST_RETRY_LIMIT, (unsigned long)delay_ms);
            } else {

                delay_ms = COOLDOWN_BACKOFF_MS;
                s_retry_count = 0;
                s_wifi_status = WIFI_STATUS_COOLDOWN;
                ESP_LOGW(TAG, "Initial %d reconnect attempts exhausted. Entering %lu s cooldown before next retry tier...",
                         FAST_RETRY_LIMIT, (unsigned long)(delay_ms / 1000));
            }

            if (s_reconnect_timer) {
                esp_timer_stop(s_reconnect_timer);
                esp_timer_start_once(s_reconnect_timer, (uint64_t)delay_ms * 1000ULL);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi link established! Acquired IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_wifi_status = WIFI_STATUS_CONNECTED;

        if (s_reconnect_timer) {
            esp_timer_stop(s_reconnect_timer);
        }

        esp_err_t sntp_err = timestamp_sntp_init();
        if (sntp_err != ESP_OK) {
            ESP_LOGW(TAG, "SNTP initialization deferred: %s", esp_err_to_name(sntp_err));
        }
    }
}

esp_err_t wifi_manager_init(void) {
    if (s_netif_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();
        if (ret == ESP_OK) {
            ret = nvs_flash_init();
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS flash init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Event loop creation failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        ESP_LOGE(TAG, "Failed to create default station netif");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_timer_create_args_t timer_args = {
        .callback = &wifi_reconnect_timer_cb,
        .name = "wifi_reconnect_tmr",
        .dispatch_method = ESP_TIMER_TASK,
    };
    ret = esp_timer_create(&timer_args, &s_reconnect_timer);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create reconnect timer: %s", esp_err_to_name(ret));
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &wifi_event_handler,
                                              NULL,
                                              NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WIFI_EVENT handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler,
                                              NULL,
                                              NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP_EVENT handler: %s", esp_err_to_name(ret));
        return ret;
    }

    s_netif_initialized = true;
    s_wifi_status = WIFI_STATUS_STANDALONE;
    ESP_LOGI(TAG, "Wi-Fi manager initialized successfully (standalone mode).");
    return ESP_OK;
}

esp_err_t wifi_manager_start(const char *ssid, const char *password) {
    if (!s_netif_initialized) {
        esp_err_t err = wifi_manager_init();
        if (err != ESP_OK) return err;
    }

    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGI(TAG, "No Wi-Fi credentials configured; operating in standalone local mode.");
        s_has_credentials = false;
        s_wifi_status = WIFI_STATUS_STANDALONE;
        return ESP_OK;
    }

    s_has_credentials = true;
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password && strlen(password) > 0) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Starting Wi-Fi station for SSID '%s' (password masked)...", ssid);
    s_retry_count = 0;
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t wifi_manager_stop(void) {
    if (!s_netif_initialized) return ESP_OK;

    if (s_reconnect_timer) {
        esp_timer_stop(s_reconnect_timer);
    }

    s_has_credentials = false;
    s_wifi_status = WIFI_STATUS_STANDALONE;
    esp_wifi_disconnect();
    return esp_wifi_stop();
}

bool wifi_manager_is_connected(void) {
    return (s_wifi_status == WIFI_STATUS_CONNECTED);
}

wifi_status_t wifi_manager_get_status(void) {
    return s_wifi_status;
}

const char *wifi_manager_get_status_str(void) {
    switch (s_wifi_status) {
        case WIFI_STATUS_UNINITIALIZED: return "UNINIT";
        case WIFI_STATUS_STANDALONE:    return "STANDALONE";
        case WIFI_STATUS_CONNECTING:    return "CONNECTING";
        case WIFI_STATUS_CONNECTED:     return "CONNECTED";
        case WIFI_STATUS_DISCONNECTED:  return "DISCONNECTED";
        case WIFI_STATUS_COOLDOWN:      return "COOLDOWN";
        default:                        return "UNKNOWN";
    }
}
