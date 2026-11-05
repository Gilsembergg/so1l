#include "timestamp.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

static const char *TAG = "TIMESTAMP";

static bool s_utc_synchronized = false;
static bool s_sntp_initialized = false;

#define UTC_MIN_VALID_EPOCH 1704067200

static void sntp_sync_notification_callback(struct timeval *tv) {
    s_utc_synchronized = true;
    if (tv) {
        ESP_LOGI(TAG, "SNTP time synchronization confirmed by server (tv_sec: %lld)", (long long)tv->tv_sec);
    } else {
        ESP_LOGI(TAG, "SNTP time synchronization confirmed by server");
    }
}

esp_err_t timestamp_sntp_init(void) {
    if (s_sntp_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SNTP client (pool.ntp.org)...");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = sntp_sync_notification_callback;

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_netif_sntp_init returned: %s", esp_err_to_name(err));
        return err;
    }

    s_sntp_initialized = true;
    return ESP_OK;
}

void timestamp_sntp_deinit(void) {
    if (s_sntp_initialized) {
        esp_netif_sntp_deinit();
        s_sntp_initialized = false;
        s_utc_synchronized = false;
        ESP_LOGI(TAG, "SNTP client stopped and deinitialized.");
    }
}

uint64_t timestamp_get_monotonic_ms(void) {
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

timestamp_info_t timestamp_now(void) {
    timestamp_info_t info = {0};
    info.uptime_ms = timestamp_get_monotonic_ms();

    bool is_synced = s_utc_synchronized && (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED);
    time_t now = time(NULL);

    if (is_synced && now >= UTC_MIN_VALID_EPOCH) {
        info.utc_sec = now;
        gmtime_r(&now, &info.utc_tm);
        info.utc_valid = true;
    } else {
        info.utc_sec = 0;
        info.utc_valid = false;
    }

    return info;
}

const char *timestamp_format_iso8601(const timestamp_info_t *ts, char *buf, size_t size) {
    if (!buf || size == 0) {
        return "";
    }

    if (ts && ts->utc_valid) {
        strftime(buf, size, "%Y-%m-%dT%H:%M:%SZ", &ts->utc_tm);
    } else {
        snprintf(buf, size, "UNSYNCHRONIZED");
    }
    return buf;
}

void timestamp_set_utc_synchronized(bool synced) {
    s_utc_synchronized = synced;
    ESP_LOGI(TAG, "UTC synchronization state manually set to: %s", synced ? "VALID" : "UNSYNCHRONIZED");
}

bool timestamp_is_utc_synchronized(void) {
    return s_utc_synchronized && (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED);
}
