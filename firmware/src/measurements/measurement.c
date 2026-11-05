#include "measurement.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "MEASUREMENT";
static uint32_t s_sample_sequence = 0;

esp_err_t measurement_subsystem_init(void) {
    ESP_LOGI(TAG, "Initializing measurement subsystem...");

    esp_err_t m_err = moisture_init();
    if (m_err != ESP_OK) {
        ESP_LOGE(TAG, "Moisture ADC init failed: %s", esp_err_to_name(m_err));
    }

    esp_err_t t_err = temperature_init();
    if (t_err != ESP_OK) {
        ESP_LOGW(TAG, "DS18B20 init reported: %s (will retry during read)", esp_err_to_name(t_err));
    }

    if (m_err != ESP_OK && t_err != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

measurement measurement_read(void) {
    measurement m = {0};

    m.sequence = ++s_sample_sequence;
    m.time_info = timestamp_now();
    m.uptime_ms = m.time_info.uptime_ms;
    m.error_flags = MEASUREMENT_OK;

    float temp_val = 0.0f;
    esp_err_t t_err = temperature_read(&temp_val);
    if (t_err == ESP_OK) {
        m.temperature_c = temp_val;
        m.temperature_valid = true;
        m.temperature_error_code = 0;
    } else {
        m.temperature_c = 0.0f;
        m.temperature_valid = false;
        m.temperature_error_code = t_err;
        if (t_err == ESP_ERR_INVALID_CRC) {
            m.error_flags |= MEASUREMENT_ERR_TEMP_CRC;
        } else {
            m.error_flags |= MEASUREMENT_ERR_TEMP_COMM;
        }
    }

    moisture_reading_t moist = moisture_read();
    if (moist.raw_valid) {
        m.moisture_raw = moist.raw;
        m.moisture_raw_valid = true;
        m.moisture_percent = moist.percent;
        m.moisture_percent_valid = moist.percent_valid;
        if (!moist.percent_valid) {
            m.error_flags |= MEASUREMENT_ERR_MOIST_UNCAL;
        }
    } else {
        m.moisture_raw = 0;
        m.moisture_raw_valid = false;
        m.moisture_percent = 0.0f;
        m.moisture_percent_valid = false;
        m.error_flags |= MEASUREMENT_ERR_MOIST_ADC;
    }

    if (!m.time_info.utc_valid) {
        m.error_flags |= MEASUREMENT_ERR_TIME_UNSYNC;
    }

    ESP_LOGI(TAG, "Seq #%" PRIu32 " | Temp: %s (%.2f °C) | Raw ADC: %d (%s) | Calib %%: %s (%.1f%%)",
             m.sequence,
             m.temperature_valid ? "VALID" : "INVALID",
             m.temperature_c,
             m.moisture_raw,
             m.moisture_raw_valid ? "OK" : "ERR",
             m.moisture_percent_valid ? "VALID" : "UNCAL",
             m.moisture_percent);

    return m;
}
