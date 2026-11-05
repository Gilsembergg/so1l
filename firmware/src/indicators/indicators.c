#include "indicators.h"
#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "INDICATORS";

#define LED_GREEN_GPIO   GPIO_NUM_3
#define LED_YELLOW_GPIO  GPIO_NUM_10
#define LED_RED_GPIO     GPIO_NUM_21

static indicator_state_t s_current_state = INDICATOR_STATE_OFF;

esp_err_t indicators_init(void) {
    uint64_t pin_mask = (1ULL << LED_GREEN_GPIO) |
                        (1ULL << LED_YELLOW_GPIO) |
                        (1ULL << LED_RED_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED GPIOs: %s", esp_err_to_name(err));
        return err;
    }

    indicators_set_raw(false, false, false);
    ESP_LOGI(TAG, "Indicators initialized (Green:GPIO3, Yellow:GPIO10, Red:GPIO21)");
    return ESP_OK;
}

void indicators_set_raw(bool green, bool yellow, bool red) {
    gpio_set_level(LED_GREEN_GPIO, green ? 1 : 0);
    gpio_set_level(LED_YELLOW_GPIO, yellow ? 1 : 0);
    gpio_set_level(LED_RED_GPIO, red ? 1 : 0);
}

void indicators_set_state(indicator_state_t state) {
    s_current_state = state;

    switch (state) {
        case INDICATOR_STATE_CRITICAL:

            indicators_set_raw(false, false, true);
            break;

        case INDICATOR_STATE_DEGRADED:

            indicators_set_raw(false, true, false);
            break;

        case INDICATOR_STATE_NORMAL:

            indicators_set_raw(true, false, false);
            break;

        case INDICATOR_STATE_STARTUP:

            indicators_set_raw(false, true, false);
            break;

        case INDICATOR_STATE_OFF:
        default:
            indicators_set_raw(false, false, false);
            break;
    }
}

void indicators_self_test(void) {
    ESP_LOGI(TAG, "Running LED indicator self-test sequence...");

    indicators_set_raw(true, false, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    indicators_set_raw(false, true, false);
    vTaskDelay(pdMS_TO_TICKS(150));
    indicators_set_raw(false, false, true);
    vTaskDelay(pdMS_TO_TICKS(150));
    indicators_set_raw(false, false, false);
    vTaskDelay(pdMS_TO_TICKS(100));
}

