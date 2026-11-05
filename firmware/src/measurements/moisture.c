#include "moisture.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MOISTURE";

#define MOISTURE_ADC_UNIT     ADC_UNIT_1
#define MOISTURE_ADC_CHANNEL  ADC_CHANNEL_0
#define NUM_SAMPLES           8

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static bool s_adc_initialized = false;

static bool s_is_calibrated = false;
static int s_raw_dry = 0;
static int s_raw_wet = 0;

esp_err_t moisture_init(void) {
    if (s_adc_initialized) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = MOISTURE_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC oneshot unit: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };
    ret = adc_oneshot_config_channel(s_adc_handle, MOISTURE_ADC_CHANNEL, &chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel 0: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        return ret;
    }

    s_adc_initialized = true;
    ESP_LOGI(TAG, "Moisture sensor ADC initialized on GPIO0 (ADC1_CH0). Calibration: %s",
             s_is_calibrated ? "SET" : "UNCALIBRATED");
    return ESP_OK;
}

moisture_reading_t moisture_read(void) {
    moisture_reading_t reading = {
        .raw = 0,
        .raw_valid = false,
        .percent = 0.0f,
        .percent_valid = false
    };

    if (!s_adc_initialized || !s_adc_handle) {
        ESP_LOGW(TAG, "ADC not initialized, attempting init...");
        if (moisture_init() != ESP_OK) {
            return reading;
        }
    }

    int sum = 0;
    int valid_samples = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        int sample = 0;
        esp_err_t err = adc_oneshot_read(s_adc_handle, MOISTURE_ADC_CHANNEL, &sample);
        if (err == ESP_OK) {
            sum += sample;
            valid_samples++;
        }
        if (i < NUM_SAMPLES - 1) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    if (valid_samples == 0) {
        ESP_LOGE(TAG, "Failed to sample ADC on channel 0");
        return reading;
    }

    reading.raw = sum / valid_samples;
    reading.raw_valid = true;

    if (s_is_calibrated && (s_raw_dry != s_raw_wet)) {

        float dry = (float)s_raw_dry;
        float wet = (float)s_raw_wet;
        float pct = ((dry - (float)reading.raw) / (dry - wet)) * 100.0f;

        if (pct < 0.0f) {
            pct = 0.0f;
        } else if (pct > 100.0f) {
            pct = 100.0f;
        }
        reading.percent = pct;
        reading.percent_valid = true;
    } else {

        reading.percent = 0.0f;
        reading.percent_valid = false;
    }

    return reading;
}

esp_err_t moisture_set_calibration(int raw_dry, int raw_wet) {
    if (raw_dry == raw_wet || raw_dry <= 0 || raw_wet <= 0) {
        ESP_LOGE(TAG, "Invalid calibration values: dry=%d, wet=%d", raw_dry, raw_wet);
        return ESP_ERR_INVALID_ARG;
    }

    s_raw_dry = raw_dry;
    s_raw_wet = raw_wet;
    s_is_calibrated = true;
    ESP_LOGI(TAG, "Moisture calibration updated: dry=%d, wet=%d", s_raw_dry, s_raw_wet);
    return ESP_OK;
}

bool moisture_is_calibrated(void) {
    return s_is_calibrated;
}
