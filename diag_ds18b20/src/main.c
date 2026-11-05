#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DS18B20_GPIO            GPIO_NUM_4
#define NUM_TEST_ITERATIONS     10
#define INTER_TEST_DELAY_MS     800

static const char *TAG = "DS18B20_DIAG";
static portMUX_TYPE s_diag_mux = portMUX_INITIALIZER_UNLOCKED;

typedef struct {
    uint64_t timestamp_ms;
    int pre_reset_level;
    int sample_70us;
    int sample_100us;
    int sample_150us;
    int sample_250us;
    int final_recovery_level;
    bool presence_detected;
    bool bus_recovered;
    bool passed;
} diag_sample_t;

static const char* level_to_str(int level) {
    return (level == 1) ? "HIGH" : "LOW ";
}

static esp_err_t diag_gpio_init(void) {
    gpio_reset_pin(DS18B20_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DS18B20_GPIO),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO%d for open-drain: %s", DS18B20_GPIO, esp_err_to_name(err));
        return err;
    }

    gpio_set_level(DS18B20_GPIO, 1);
    esp_rom_delay_us(100);
    return ESP_OK;
}

static void run_single_reset_test(diag_sample_t *result) {
    result->timestamp_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);

    result->pre_reset_level = gpio_get_level(DS18B20_GPIO);

    gpio_set_level(DS18B20_GPIO, 0);
    esp_rom_delay_us(480);

    portENTER_CRITICAL(&s_diag_mux);
    gpio_set_level(DS18B20_GPIO, 1);

    esp_rom_delay_us(70);
    result->sample_70us = gpio_get_level(DS18B20_GPIO);

    esp_rom_delay_us(30);
    result->sample_100us = gpio_get_level(DS18B20_GPIO);

    esp_rom_delay_us(50);
    result->sample_150us = gpio_get_level(DS18B20_GPIO);

    esp_rom_delay_us(100);
    result->sample_250us = gpio_get_level(DS18B20_GPIO);
    portEXIT_CRITICAL(&s_diag_mux);

    esp_rom_delay_us(250);

    result->final_recovery_level = gpio_get_level(DS18B20_GPIO);

    result->presence_detected = (result->sample_70us == 0 || result->sample_100us == 0);
    result->bus_recovered = (result->final_recovery_level == 1);
    result->passed = (result->pre_reset_level == 1) && result->presence_detected && result->bus_recovered;
}

void app_main(void) {

    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n");
    printf("========================================================================\n");
    printf("     DS18B20 ISOLATED RESET & PRESENCE DIAGNOSTIC TEST (ESP32-C3)       \n");
    printf("========================================================================\n");
    printf(" Pin: GPIO4 (Open-Drain OD) | External Pull-Up: 4.7 kOhm to 3.3V\n");
    printf(" Repetitions: %d iterations | Sampling at: +70us, +100us, +150us, +250us\n", NUM_TEST_ITERATIONS);
    printf(" Baud Rate: 115200\n");
    printf("------------------------------------------------------------------------\n\n");

    esp_err_t err = diag_gpio_init();
    if (err != ESP_OK) {
        printf("[FATAL] GPIO4 initialization failed! Error: %s\n", esp_err_to_name(err));
        return;
    }

    diag_sample_t samples[NUM_TEST_ITERATIONS];
    int pass_count = 0;
    int stuck_low_count = 0;
    int no_presence_count = 0;

    for (int i = 0; i < NUM_TEST_ITERATIONS; i++) {
        run_single_reset_test(&samples[i]);

        uint32_t sec = (uint32_t)(samples[i].timestamp_ms / 1000ULL);
        uint32_t ms = (uint32_t)(samples[i].timestamp_ms % 1000ULL);

        const char *result_str = "UNKNOWN";
        if (samples[i].passed) {
            pass_count++;
            result_str = "[PASS] Presence Pulse Confirmed";
        } else if (samples[i].pre_reset_level == 0 || samples[i].final_recovery_level == 0) {
            stuck_low_count++;
            result_str = "[FAIL] Bus Stuck LOW (Check Pull-Up / Short to GND)";
        } else if (!samples[i].presence_detected) {
            no_presence_count++;
            result_str = "[FAIL] No Presence Pulse (Bus Remained HIGH)";
        } else {
            result_str = "[FAIL] Recovery Failed";
        }

        printf("[%03" PRIu32 ".%03" PRIu32 "s] Test #%02d | Pre:%s | 70us:%s 100us:%s 150us:%s 250us:%s | Post:%s => %s\n",
               sec, ms,
               i + 1,
               level_to_str(samples[i].pre_reset_level),
               level_to_str(samples[i].sample_70us),
               level_to_str(samples[i].sample_100us),
               level_to_str(samples[i].sample_150us),
               level_to_str(samples[i].sample_250us),
               level_to_str(samples[i].final_recovery_level),
               result_str);

        vTaskDelay(pdMS_TO_TICKS(INTER_TEST_DELAY_MS));
    }

    float pass_rate = ((float)pass_count / (float)NUM_TEST_ITERATIONS) * 100.0f;

    printf("\n");
    printf("========================================================================\n");
    printf("                         DIAGNOSTIC SUMMARY                             \n");
    printf("========================================================================\n");
    printf(" Total Tests Executed:         %d\n", NUM_TEST_ITERATIONS);
    printf(" Presence Pulse Confirmed:     %d (%.1f%%)\n", pass_count, pass_rate);
    printf(" No Presence (Bus High):       %d\n", no_presence_count);
    printf(" Bus Stuck Low Faults:         %d\n", stuck_low_count);
    printf("------------------------------------------------------------------------\n");

    if (pass_count == NUM_TEST_ITERATIONS) {
        printf(" VERDICT: SENSOR CONFIRMED OPERATIONAL [100%% SUCCESS]\n");
        printf(" The DS18B20 actively pulls the 1-Wire bus LOW during the presence\n");
        printf(" window (70-150 us) and releases it back to HIGH before 500 us.\n");
        printf(" Hardware connection, pull-up resistor, and probe are verified OK.\n");
    } else if (pass_count > 0) {
        printf(" VERDICT: INTERMITTENT RESPONSE (%.1f%% Success)\n", pass_rate);
        printf(" Sensor responds intermittently. Check for loose breadboard jumper\n");
        printf(" wires, unstable 3.3V power, or excessive bus capacitance.\n");
    } else if (stuck_low_count > 0) {
        printf(" VERDICT: BUS WIRING FAULT (Bus Stuck LOW)\n");
        printf(" The bus reads LOW before reset or fails to recover to HIGH.\n");
        printf(" Check: 1) Missing 4.7 kOhm pull-up to 3.3V, or\n");
        printf("        2) DATA line shorted to GND, or\n");
        printf("        3) Probe reversed (GND and VCC swapped).\n");
    } else {
        printf(" VERDICT: NO SENSOR RESPONSE (Bus Remained Constant HIGH)\n");
        printf(" The 4.7 kOhm pull-up is working (bus is HIGH), but the DS18B20 is\n");
        printf(" not generating a presence pulse.\n");
        printf(" Check: 1) DS18B20 VCC wire (red) connected to 3.3V,\n");
        printf("        2) DS18B20 GND wire (black) connected to GND,\n");
        printf("        3) DS18B20 DATA wire (yellow/blue) connected to GPIO4.\n");
    }
    printf("========================================================================\n\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

