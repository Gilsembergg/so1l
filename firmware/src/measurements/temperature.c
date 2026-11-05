#include "temperature.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DS18B20";

#define DS18B20_GPIO               GPIO_NUM_4
#define DS18B20_CMD_CONVERT_T      0x44
#define DS18B20_CMD_SKIP_ROM       0xCC
#define DS18B20_CMD_READ_SCRATCH   0xBE

static portMUX_TYPE s_onewire_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized = false;

static inline void onewire_low(void) {
    gpio_set_level(DS18B20_GPIO, 0);
}

static inline void onewire_release(void) {
    gpio_set_level(DS18B20_GPIO, 1);
}

static inline int onewire_read(void) {
    return gpio_get_level(DS18B20_GPIO);
}

static bool onewire_reset(void) {

    if (onewire_read() == 0) {
        ESP_LOGW(TAG, "1-Wire bus stuck LOW on GPIO%d before reset (short to GND or missing 4.7k pull-up)", DS18B20_GPIO);
        return false;
    }

    onewire_low();
    esp_rom_delay_us(480);

    portENTER_CRITICAL(&s_onewire_mux);
    onewire_release();
    esp_rom_delay_us(70);
    int presence = onewire_read();
    portEXIT_CRITICAL(&s_onewire_mux);

    esp_rom_delay_us(410);

    int post_state = onewire_read();

    if (presence != 0) {
        ESP_LOGW(TAG, "1-Wire reset: no presence pulse detected on GPIO%d (bus remained HIGH)", DS18B20_GPIO);
        return false;
    }

    if (post_state == 0) {
        ESP_LOGW(TAG, "1-Wire reset: bus stuck LOW after recovery period on GPIO%d", DS18B20_GPIO);
        return false;
    }

    ESP_LOGD(TAG, "1-Wire reset: presence pulse successfully detected on GPIO%d", DS18B20_GPIO);
    return true;
}

static void onewire_write_bit(int bit) {
    if (bit) {

        portENTER_CRITICAL(&s_onewire_mux);
        onewire_low();
        esp_rom_delay_us(6);
        onewire_release();
        portEXIT_CRITICAL(&s_onewire_mux);
        esp_rom_delay_us(64);
    } else {

        portENTER_CRITICAL(&s_onewire_mux);
        onewire_low();
        esp_rom_delay_us(60);
        onewire_release();
        portEXIT_CRITICAL(&s_onewire_mux);
        esp_rom_delay_us(10);
    }
}

static int onewire_read_bit(void) {

    portENTER_CRITICAL(&s_onewire_mux);
    onewire_low();
    esp_rom_delay_us(2);
    onewire_release();
    esp_rom_delay_us(10);
    int bit = onewire_read();
    portEXIT_CRITICAL(&s_onewire_mux);

    esp_rom_delay_us(55);
    return bit;
}

static void onewire_write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        onewire_write_bit((byte >> i) & 1);
    }
}

static uint8_t onewire_read_byte(void) {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        if (onewire_read_bit()) {
            byte |= (1 << i);
        }
    }
    return byte;
}

static uint8_t onewire_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            inbyte >>= 1;
        }
    }
    return crc;
}

esp_err_t temperature_init(void) {

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
        ESP_LOGE(TAG, "Failed to configure GPIO%d in open-drain mode: %s", DS18B20_GPIO, esp_err_to_name(err));
        return err;
    }

    onewire_release();
    esp_rom_delay_us(100);

    if (!onewire_reset()) {
        ESP_LOGW(TAG, "DS18B20 presence check failed during init on GPIO%d", DS18B20_GPIO);
        s_initialized = false;
        return ESP_ERR_NOT_FOUND;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "DS18B20 detected and initialized on GPIO%d (Open-Drain mode)", DS18B20_GPIO);
    return ESP_OK;
}

esp_err_t temperature_read(float *out_celsius) {
    if (!out_celsius) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        esp_err_t err = temperature_init();
        if (err != ESP_OK) {
            return err;
        }
    }

    if (!onewire_reset()) {
        ESP_LOGW(TAG, "DS18B20 not responding before conversion on GPIO%d", DS18B20_GPIO);
        s_initialized = false;
        return ESP_ERR_NOT_FOUND;
    }

    onewire_write_byte(DS18B20_CMD_SKIP_ROM);
    onewire_write_byte(DS18B20_CMD_CONVERT_T);

    vTaskDelay(pdMS_TO_TICKS(800));

    if (!onewire_reset()) {
        ESP_LOGW(TAG, "DS18B20 not responding after conversion on GPIO%d", DS18B20_GPIO);
        s_initialized = false;
        return ESP_ERR_NOT_FOUND;
    }

    onewire_write_byte(DS18B20_CMD_SKIP_ROM);
    onewire_write_byte(DS18B20_CMD_READ_SCRATCH);

    uint8_t scratchpad[9] = {0};
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = onewire_read_byte();
    }

    bool all_zeros = true;
    bool all_ones = true;
    for (int i = 0; i < 9; i++) {
        if (scratchpad[i] != 0x00) all_zeros = false;
        if (scratchpad[i] != 0xFF) all_ones = false;
    }
    if (all_zeros || all_ones) {
        ESP_LOGW(TAG, "DS18B20 invalid scratchpad response on GPIO%d (all 0x%02X)",
                 DS18B20_GPIO, all_zeros ? 0x00 : 0xFF);
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t computed_crc = onewire_crc8(scratchpad, 8);
    if (computed_crc != scratchpad[8]) {
        ESP_LOGE(TAG, "DS18B20 CRC error on GPIO%d: read 0x%02X, computed 0x%02X",
                 DS18B20_GPIO, scratchpad[8], computed_crc);
        return ESP_ERR_INVALID_CRC;
    }

    int16_t raw_temp = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);
    float temp_c = (float)raw_temp / 16.0f;

    if (temp_c < -55.0f || temp_c > 125.0f) {
        ESP_LOGE(TAG, "DS18B20 reading out of physical bounds on GPIO%d: %.2f °C", DS18B20_GPIO, temp_c);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *out_celsius = temp_c;
    ESP_LOGD(TAG, "DS18B20 reading on GPIO%d: %.2f °C (raw=0x%04X, CRC=0x%02X)",
             DS18B20_GPIO, temp_c, (uint16_t)raw_temp, computed_crc);
    return ESP_OK;
}
