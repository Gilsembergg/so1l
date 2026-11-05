#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "measurement.h"
#include "indicators.h"
#include "display.h"
#include "network.h"
#include "wifi.h"

static const char *TAG = "SO1L_MAIN";

#define SO1L_MEASUREMENT_INTERVAL_MS 5000
#define DISPLAY_RETRY_INTERVAL_CYCLES 6

static indicator_state_t evaluate_device_state(const measurement *m) {
    if (!m->temperature_valid && !m->moisture_raw_valid) {
        return INDICATOR_STATE_CRITICAL;
    }
    if (!m->temperature_valid || !m->moisture_raw_valid || !m->moisture_percent_valid) {
        return INDICATOR_STATE_DEGRADED;
    }
    return INDICATOR_STATE_NORMAL;
}

static const char *device_state_to_str(indicator_state_t state) {
    switch (state) {
        case INDICATOR_STATE_NORMAL:   return "NORMAL";
        case INDICATOR_STATE_DEGRADED: return "DEGRADED";
        case INDICATOR_STATE_CRITICAL: return "CRITICAL";
        case INDICATOR_STATE_STARTUP:  return "STARTUP";
        default:                       return "UNKNOWN";
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "      So1l Standalone Soil-Sensing Module         ");
    ESP_LOGI(TAG, "==================================================");

    ESP_LOGI(TAG, "[1/5] Initializing LED indicators...");
    indicators_init();
    indicators_self_test();
    indicators_set_state(INDICATOR_STATE_STARTUP);

    ESP_LOGI(TAG, "[2/5] Initializing SSD1306 OLED display...");
    esp_err_t disp_err = display_init();
    if (disp_err != ESP_OK) {
        ESP_LOGW(TAG, "OLED display unavailable (%s); operating in headless mode.", esp_err_to_name(disp_err));
    }

    ESP_LOGI(TAG, "[3/5] Initializing measurement subsystem...");
    measurement_subsystem_init();

    ESP_LOGI(TAG, "[4/5] Initializing network subsystem...");
    network_init();

    ESP_LOGI(TAG, "[5/5] Setup complete. Entering main periodic measurement loop (Interval: %d ms).",
             SO1L_MEASUREMENT_INTERVAL_MS);

    uint32_t cycle_counter = 0;

    while (1) {
        cycle_counter++;

        measurement m = measurement_read();

        indicator_state_t state = evaluate_device_state(&m);
        indicators_set_state(state);

        if (display_is_available()) {
            display_show_measurement(&m, device_state_to_str(state));
        } else if ((cycle_counter % DISPLAY_RETRY_INTERVAL_CYCLES) == 0) {

            display_init();
            if (display_is_available()) {
                display_show_measurement(&m, device_state_to_str(state));
            }
        }

        network_publish_measurement(&m);

        vTaskDelay(pdMS_TO_TICKS(SO1L_MEASUREMENT_INTERVAL_MS));
    }
}
