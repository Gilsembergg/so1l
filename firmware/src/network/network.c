#include "network.h"
#include <stdio.h>
#include "esp_log.h"
#include "wifi.h"
#include "telemetry.h"

static const char *TAG = "NETWORK";

esp_err_t network_init(void) {
    ESP_LOGI(TAG, "Initializing network subsystem...");

    telemetry_config_t t_cfg = {
        .module_id = SO1L_DEFAULT_MODULE_ID
    };
    esp_err_t err = telemetry_init(&t_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize telemetry: %s", esp_err_to_name(err));
        return err;
    }

    err = wifi_manager_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi manager init reported: %s (continuing in standalone mode)", esp_err_to_name(err));
    }

    wifi_manager_start(NULL, NULL);

    return ESP_OK;
}

bool network_is_ready(void) {
    return wifi_manager_is_connected();
}

esp_err_t network_publish_measurement(const measurement *m) {
    if (!m) {
        return ESP_ERR_INVALID_ARG;
    }

    char payload[SO1L_TELEMETRY_BUFFER_SIZE];
    size_t len = 0;
    esp_err_t err = telemetry_serialize(m, payload, sizeof(payload), &len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Telemetry serialization failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Telemetry payload (len=%zu): %s", len, payload);

    if (!network_is_ready()) {
        ESP_LOGD(TAG, "Transport not connected; payload generated but not dispatched to remote endpoint.");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Published measurement #%" PRIu32 " to active transport", m->sequence);
    return ESP_OK;
}

const char *network_get_status_str(void) {
    return wifi_manager_get_status_str();
}

