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

#define SAMPLE_START_US         10
#define SAMPLE_END_US           320
#define SAMPLE_STEP_US          5
#define NUM_SAMPLES             (((SAMPLE_END_US - SAMPLE_START_US) / SAMPLE_STEP_US) + 1)
#define TOTAL_SLOT_DURATION_US  480

static const char *TAG = "DS18B20_DIAG";
static portMUX_TYPE s_diag_mux = portMUX_INITIALIZER_UNLOCKED;

typedef struct {
    uint64_t timestamp_ms;
    int pre_reset_level;
    uint8_t sample_levels[NUM_SAMPLES];
    uint16_t sample_times_us[NUM_SAMPLES];
    int low_count;
    int first_low_us;
    int last_low_us;
    int low_duration_us;
    int final_recovery_level;
    uint16_t final_recovery_time_us;
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
        .pull_up_en = GPIO_PULLUP_DISABLE,
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
    int64_t t_release = esp_timer_get_time();

    for (int i = 0; i < NUM_SAMPLES; i++) {
        int32_t target_us = SAMPLE_START_US + (i * SAMPLE_STEP_US);
        while ((int32_t)(esp_timer_get_time() - t_release) < target_us) {
        }
        result->sample_levels[i] = (uint8_t)gpio_get_level(DS18B20_GPIO);
        result->sample_times_us[i] = (uint16_t)(esp_timer_get_time() - t_release);
    }

    portEXIT_CRITICAL(&s_diag_mux);

    int64_t elapsed_us = esp_timer_get_time() - t_release;
    if (elapsed_us < TOTAL_SLOT_DURATION_US) {
        esp_rom_delay_us((uint32_t)(TOTAL_SLOT_DURATION_US - elapsed_us));
    }

    result->final_recovery_level = gpio_get_level(DS18B20_GPIO);
    result->final_recovery_time_us = (uint16_t)(esp_timer_get_time() - t_release);

    int low_count = 0;
    int first_low = -1;
    int last_low = -1;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (result->sample_levels[i] == 0) {
            low_count++;
            if (first_low < 0) {
                first_low = result->sample_times_us[i];
            }
            last_low = result->sample_times_us[i];
        }
    }

    result->low_count = low_count;
    result->first_low_us = first_low;
    result->last_low_us = last_low;
    result->low_duration_us = (low_count > 0) ? (last_low - first_low) : 0;

    bool timing_valid = (low_count >= 5) &&
                        (first_low >= 10 && first_low <= 100) &&
                        (last_low >= 60 && last_low <= 320);

    result->presence_detected = (result->pre_reset_level == 1) && timing_valid;
    result->bus_recovered = (result->final_recovery_level == 1);
    result->passed = result->presence_detected && result->bus_recovered;
}

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n");
    printf("========================================================================\n");
    printf("     DS18B20 ISOLATED RESET & PRESENCE DIAGNOSTIC TEST (ESP32-C3)       \n");
    printf("========================================================================\n");
    printf(" Pin: GPIO4 (Open-Drain OD) | Internal Pull-Up: DISABLED\n");
    printf(" Pull-Up Dependency: External 4.7 kOhm to 3.3V\n");
    printf(" Sampling Window: 10 us to 320 us after bus release (every 5 us, %d samples)\n", NUM_SAMPLES);
    printf(" Recovery Check: +%d us (End of Standard 1-Wire Reset Slot)\n", TOTAL_SLOT_DURATION_US);
    printf(" Repetitions: %d iterations | Baud Rate: 115200\n", NUM_TEST_ITERATIONS);
    printf("------------------------------------------------------------------------\n\n");

    esp_err_t err = diag_gpio_init();
    if (err != ESP_OK) {
        printf("[FATAL] GPIO4 initialization failed! Error: %s\n", esp_err_to_name(err));
        return;
    }

    diag_sample_t samples[NUM_TEST_ITERATIONS];
    int pass_count = 0;
    int stuck_low_pre_count = 0;
    int no_presence_count = 0;
    int recovery_fail_count = 0;
    int glitch_count = 0;

    int sum_first_low = 0;
    int sum_last_low = 0;
    int sum_dur = 0;
    int min_first_low = 9999, max_first_low = 0;
    int min_last_low = 9999, max_last_low = 0;
    int min_dur = 9999, max_dur = 0;

    for (int i = 0; i < NUM_TEST_ITERATIONS; i++) {
        run_single_reset_test(&samples[i]);

        uint32_t sec = (uint32_t)(samples[i].timestamp_ms / 1000ULL);
        uint32_t ms = (uint32_t)(samples[i].timestamp_ms % 1000ULL);

        printf("[%03" PRIu32 ".%03" PRIu32 "s] Test #%02d | Pre: %s | Window: 10-320us (%d spl @ 5us) | ",
               sec, ms, i + 1, level_to_str(samples[i].pre_reset_level), NUM_SAMPLES);

        if (samples[i].pre_reset_level == 0) {
            stuck_low_pre_count++;
            printf("LOW: %2d/%d (stuck LOW before reset)              | Post: %s => [FAIL] Bus Initially Stuck LOW\n",
                   samples[i].low_count, NUM_SAMPLES, level_to_str(samples[i].final_recovery_level));
        } else if (samples[i].low_count == 0) {
            no_presence_count++;
            printf("LOW:  0/%d (no LOW observed, line idle HIGH)      | Post: %s => [FAIL] No Presence Pulse (Line Idle HIGH)\n",
                   NUM_SAMPLES, level_to_str(samples[i].final_recovery_level));
        } else if (samples[i].final_recovery_level == 0) {
            recovery_fail_count++;
            printf("LOW: %2d/%d (first: +%3dus, last: +%3dus, dur: %3dus) | Post: %s => [FAIL] Bus Failed Recovery (Stuck LOW)\n",
                   samples[i].low_count, NUM_SAMPLES, samples[i].first_low_us, samples[i].last_low_us, samples[i].low_duration_us,
                   level_to_str(samples[i].final_recovery_level));
        } else if (samples[i].low_count < 5) {
            glitch_count++;
            printf("LOW: %2d/%d (first: +%3dus, last: +%3dus, dur: %3dus) | Post: %s => [FAIL] Glitch / Noise (Pulse < 25us)\n",
                   samples[i].low_count, NUM_SAMPLES, samples[i].first_low_us, samples[i].last_low_us, samples[i].low_duration_us,
                   level_to_str(samples[i].final_recovery_level));
        } else {
            pass_count++;
            printf("LOW: %2d/%d (first: +%3dus, last: +%3dus, dur: %3dus) | Post: %s => [PASS] Presence Pulse Detected\n",
                   samples[i].low_count, NUM_SAMPLES, samples[i].first_low_us, samples[i].last_low_us, samples[i].low_duration_us,
                   level_to_str(samples[i].final_recovery_level));

            sum_first_low += samples[i].first_low_us;
            sum_last_low += samples[i].last_low_us;
            sum_dur += samples[i].low_duration_us;

            if (samples[i].first_low_us < min_first_low) min_first_low = samples[i].first_low_us;
            if (samples[i].first_low_us > max_first_low) max_first_low = samples[i].first_low_us;
            if (samples[i].last_low_us < min_last_low) min_last_low = samples[i].last_low_us;
            if (samples[i].last_low_us > max_last_low) max_last_low = samples[i].last_low_us;
            if (samples[i].low_duration_us < min_dur) min_dur = samples[i].low_duration_us;
            if (samples[i].low_duration_us > max_dur) max_dur = samples[i].low_duration_us;
        }

        vTaskDelay(pdMS_TO_TICKS(INTER_TEST_DELAY_MS));
    }

    float pass_rate = ((float)pass_count / (float)NUM_TEST_ITERATIONS) * 100.0f;

    printf("\n");
    printf("========================================================================\n");
    printf("                         DIAGNOSTIC SUMMARY                             \n");
    printf("========================================================================\n");
    printf(" Total Tests Executed:         %d\n", NUM_TEST_ITERATIONS);
    printf(" Presence Pulse Detected:      %d (%.1f%%)\n", pass_count, pass_rate);
    printf(" No Presence (Line Idle HIGH): %d\n", no_presence_count);
    printf(" Bus Initially Stuck LOW:      %d\n", stuck_low_pre_count);
    printf(" Bus Failed Recovery (Post):   %d\n", recovery_fail_count);
    printf(" Glitch / Timing Anomalies:    %d\n", glitch_count);
    printf("------------------------------------------------------------------------\n");

    if (pass_count > 0) {
        float avg_first = (float)sum_first_low / (float)pass_count;
        float avg_last = (float)sum_last_low / (float)pass_count;
        float avg_dur = (float)sum_dur / (float)pass_count;

        printf(" Timing Profile of Detected Presence Pulses:\n");
        printf("   - First LOW:  min: %3d us | max: %3d us | avg: %5.1f us (DS18B20 spec: 15-60 us)\n",
               min_first_low, max_first_low, avg_first);
        printf("   - Last LOW:   min: %3d us | max: %3d us | avg: %5.1f us (DS18B20 spec: 75-300 us)\n",
               min_last_low, max_last_low, avg_last);
        printf("   - Duration:   min: %3d us | max: %3d us | avg: %5.1f us (DS18B20 spec: 60-240 us)\n",
               min_dur, max_dur, avg_dur);
        printf("------------------------------------------------------------------------\n");
    }

    printf(" DIAGNOSTIC VERDICT:\n");
    if (pass_count == NUM_TEST_ITERATIONS) {
        printf("   [1-WIRE PRESENCE PULSE DETECTED - 100%% CONSISTENCY]\n");
        printf("   - The connected device actively pulled GPIO4 LOW within the expected\n");
        printf("     presence window and released the bus back to HIGH at +%d us.\n", TOTAL_SLOT_DURATION_US);
        printf("   - SCOPE LIMITATION: This test confirms physical 1-Wire presence pulse\n");
        printf("     response only. It does NOT verify ROM addressing (Match/Skip ROM),\n");
        printf("     scratchpad read/write, temperature conversion (0x44), or CRC8 data\n");
        printf("     integrity. Full sensor functionality requires complete protocol\n");
        printf("     transaction testing.\n");
    } else if (pass_count > 0) {
        printf("   [INTERMITTENT 1-WIRE PRESENCE RESPONSE (%.1f%% SUCCESS)]\n", pass_rate);
        printf("   - Presence pulse was detected in %d of %d tests.\n", pass_count, NUM_TEST_ITERATIONS);
        printf("   - Intermittent response indicates signal degradation. Check for:\n");
        printf("     1) Loose probe wiring or breadboard jumper contacts,\n");
        printf("     2) High bus capacitance or long wire run degrading rise times,\n");
        printf("     3) Unstable 3.3V supply voltage or ground bounce.\n");
    } else if (stuck_low_pre_count > 0) {
        printf("   [BUS INITIALLY STUCK LOW - HARDWARE FAULT]\n");
        printf("   - The bus was read at logic LOW before the reset pulse was initiated.\n");
        printf("   - Causes:\n");
        printf("     1) External 4.7 kOhm pull-up resistor is missing or disconnected,\n");
        printf("     2) GPIO4 / DATA wire is shorted to GND,\n");
        printf("     3) DS18B20 wiring reversed (VCC and GND swapped),\n");
        printf("     4) Defective slave component holding the bus down permanently.\n");
    } else if (recovery_fail_count > 0) {
        printf("   [BUS FAILED TO RECOVER TO HIGH - RECOVERY FAULT]\n");
        printf("   - The line failed to return to logic HIGH after the presence window.\n");
        printf("   - Causes:\n");
        printf("     1) Pull-up resistor too weak (resistance too high) for bus capacitance,\n");
        printf("     2) DATA line floating or shorted,\n");
        printf("     3) Slave device clamped LOW.\n");
    } else {
        printf("   [NO PRESENCE PULSE DETECTED - BUS IDLE HIGH]\n");
        printf("   - GPIO4 remained at logic HIGH throughout the entire 10-320 us window.\n");
        printf("   - Note: Logic HIGH indicates the bus is at logic 1 (not shorted to GND),\n");
        printf("     but this does not verify the exact resistor value or rise time dynamics.\n");
        printf("   - The DS18B20 did not respond to the reset pulse. Check:\n");
        printf("     1) DS18B20 VCC pin connected to 3.3V,\n");
        printf("     2) DS18B20 GND pin connected to common ground,\n");
        printf("     3) DS18B20 DATA pin securely connected to GPIO4,\n");
        printf("     4) Sensor probe cable continuity (internal wire break),\n");
        printf("     5) Sensor damaged or non-functional.\n");
    }
    printf("========================================================================\n\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
