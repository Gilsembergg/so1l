#include "telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include "esp_err.h"
#include "esp_log.h"
#include "measurement.h"
#include "timestamp.h"

static const char *TAG = "TELEMETRY";
static char s_module_id[48] = SO1L_DEFAULT_MODULE_ID;

static bool is_valid_module_id(const char *id) {
    if (!id || id[0] == '\0') {
        return false;
    }
    size_t len = strlen(id);
    if (len > 32) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)id[i];
        if (!isalnum(c) && c != '-' && c != '_' && c != '.') {
            return false;
        }
    }
    return true;
}

static void sanitize_module_id(const char *src, char *dst, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (!src || !is_valid_module_id(src)) {
        strncpy(dst, SO1L_DEFAULT_MODULE_ID, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return;
    }
    size_t d = 0;
    for (size_t s = 0; src[s] != '\0' && d < dst_size - 1; s++) {
        char c = src[s];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') {
            dst[d++] = c;
        } else {
            dst[d++] = '_';
        }
    }
    dst[d] = '\0';
}

esp_err_t telemetry_init(const telemetry_config_t *config) {
    if (config && config->module_id) {
        sanitize_module_id(config->module_id, s_module_id, sizeof(s_module_id));
    } else {
        strncpy(s_module_id, SO1L_DEFAULT_MODULE_ID, sizeof(s_module_id) - 1);
        s_module_id[sizeof(s_module_id) - 1] = '\0';
    }
    ESP_LOGI(TAG, "Telemetry initialized (Module ID: '%s', Schema: '%s')",
             s_module_id, SO1L_TELEMETRY_SCHEMA_VERSION);
    return ESP_OK;
}

esp_err_t telemetry_serialize(const measurement *m, char *out_buf, size_t buf_size, size_t *out_len) {
    if (!m || !out_buf || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char utc_field[48];
    if (m->time_info.utc_valid) {
        char iso_buf[32];
        timestamp_format_iso8601(&m->time_info, iso_buf, sizeof(iso_buf));
        snprintf(utc_field, sizeof(utc_field), "\"%s\"", iso_buf);
    } else {
        snprintf(utc_field, sizeof(utc_field), "null");
    }

    char temp_field[24];
    if (m->temperature_valid) {
        snprintf(temp_field, sizeof(temp_field), "%.2f", m->temperature_c);
    } else {
        snprintf(temp_field, sizeof(temp_field), "null");
    }

    char raw_moist_field[24];
    if (m->moisture_raw_valid) {
        snprintf(raw_moist_field, sizeof(raw_moist_field), "%d", m->moisture_raw);
    } else {
        snprintf(raw_moist_field, sizeof(raw_moist_field), "null");
    }

    char calib_moist_field[24];
    if (m->moisture_percent_valid) {
        snprintf(calib_moist_field, sizeof(calib_moist_field), "%.2f", m->moisture_percent);
    } else {
        snprintf(calib_moist_field, sizeof(calib_moist_field), "null");
    }

    int written = snprintf(out_buf, buf_size,
        "{"
            "\"schema_version\":\"%s\","
            "\"module_id\":\"%s\","
            "\"sequence\":%" PRIu32 ","
            "\"uptime_ms\":%llu,"
            "\"utc_timestamp\":%s,"
            "\"temperature\":{"
                "\"valid\":%s,"
                "\"celsius\":%s,"
                "\"error_code\":%d"
            "},"
            "\"moisture\":{"
                "\"raw_valid\":%s,"
                "\"raw_adc\":%s,"
                "\"calibrated_percent\":%s"
            "},"
            "\"status\":{"
                "\"error_flags\":%" PRIu32
            "}"
        "}",
        SO1L_TELEMETRY_SCHEMA_VERSION,
        s_module_id,
        m->sequence,
        (unsigned long long)m->uptime_ms,
        utc_field,
        m->temperature_valid ? "true" : "false",
        temp_field,
        m->temperature_error_code,
        m->moisture_raw_valid ? "true" : "false",
        raw_moist_field,
        calib_moist_field,
        m->error_flags
    );

    if (written < 0 || (size_t)written >= buf_size) {
        ESP_LOGE(TAG, "Telemetry buffer overflow (required %d bytes, available %zu bytes)", written, buf_size);
        return ESP_ERR_NO_MEM;
    }

    if (out_len) {
        *out_len = (size_t)written;
    }

    return ESP_OK;
}
