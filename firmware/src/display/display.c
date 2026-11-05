#include "display.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SSD1306";

#define I2C_SDA_GPIO            GPIO_NUM_6
#define I2C_SCL_GPIO            GPIO_NUM_7
#define I2C_PORT_NUM            I2C_NUM_0
#define SSD1306_I2C_ADDR_DEF    0x3C
#define SSD1306_I2C_ADDR_ALT    0x3D
#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          64
#define SSD1306_PAGES           (SSD1306_HEIGHT / 8)
#define SSD1306_BUFFER_SIZE     (SSD1306_WIDTH * SSD1306_PAGES)
#define I2C_PROBE_TIMEOUT_MS    50

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_oled_dev = NULL;
static bool s_display_ready = false;
static uint8_t s_framebuffer[SSD1306_BUFFER_SIZE];

static const uint8_t s_font5x8[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x4f, 0x00, 0x00},
    {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7f, 0x14, 0x7f, 0x14},
    {0x24, 0x2a, 0x7f, 0x2a, 0x12},
    {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50},
    {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1c, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1c, 0x00},
    {0x14, 0x08, 0x3e, 0x08, 0x14},
    {0x08, 0x08, 0x3e, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00},
    {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3e, 0x51, 0x49, 0x45, 0x3e},
    {0x00, 0x42, 0x7f, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31},
    {0x18, 0x14, 0x12, 0x7f, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e},
    {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00},
    {0x08, 0x14, 0x22, 0x41, 0x00},
    {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08},
    {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3e},
    {0x7e, 0x11, 0x11, 0x11, 0x7e},
    {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22},
    {0x7f, 0x41, 0x41, 0x22, 0x1c},
    {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01},
    {0x3e, 0x41, 0x49, 0x49, 0x7a},
    {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0x00, 0x41, 0x7f, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3f, 0x01},
    {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40},
    {0x7f, 0x02, 0x0c, 0x02, 0x7f},
    {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e},
    {0x7f, 0x09, 0x09, 0x09, 0x06},
    {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f},
    {0x1f, 0x20, 0x40, 0x20, 0x1f},
    {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
    {0x00, 0x7f, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20},
    {0x00, 0x41, 0x41, 0x7f, 0x00},
    {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x01, 0x02, 0x04, 0x00},
    {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7f, 0x48, 0x44, 0x44, 0x38},
    {0x38, 0x44, 0x44, 0x44, 0x20},
    {0x38, 0x44, 0x44, 0x48, 0x7f},
    {0x38, 0x54, 0x54, 0x54, 0x18},
    {0x08, 0x7e, 0x09, 0x01, 0x02},
    {0x0c, 0x52, 0x52, 0x52, 0x3e},
    {0x7f, 0x08, 0x04, 0x04, 0x78},
    {0x00, 0x44, 0x7d, 0x40, 0x00},
    {0x20, 0x40, 0x44, 0x3d, 0x00},
    {0x7f, 0x10, 0x28, 0x44, 0x00},
    {0x00, 0x41, 0x7f, 0x40, 0x00},
    {0x7c, 0x04, 0x18, 0x04, 0x78},
    {0x7c, 0x08, 0x04, 0x04, 0x78},
    {0x38, 0x44, 0x44, 0x44, 0x38},
    {0x7c, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7c},
    {0x7c, 0x08, 0x04, 0x04, 0x08},
    {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3f, 0x44, 0x40, 0x20},
    {0x3c, 0x40, 0x40, 0x20, 0x7c},
    {0x1c, 0x20, 0x40, 0x20, 0x1c},
    {0x3c, 0x40, 0x30, 0x40, 0x3c},
    {0x44, 0x28, 0x10, 0x28, 0x44},
    {0x0c, 0x50, 0x50, 0x50, 0x3c},
    {0x44, 0x64, 0x54, 0x4c, 0x44},
    {0x00, 0x08, 0x36, 0x41, 0x00},
    {0x00, 0x00, 0x7f, 0x00, 0x00},
    {0x00, 0x41, 0x36, 0x08, 0x00},
    {0x08, 0x08, 0x2a, 0x1c, 0x08},
};

static esp_err_t ssd1306_send_cmd(uint8_t cmd) {
    if (!s_oled_dev) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_transmit(s_oled_dev, buf, sizeof(buf), 100);
}

static esp_err_t ssd1306_send_cmd_list(const uint8_t *cmds, size_t count) {
    for (size_t i = 0; i < count; i++) {
        esp_err_t err = ssd1306_send_cmd(cmds[i]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

static esp_err_t ssd1306_update_screen(void) {
    if (!s_oled_dev) return ESP_ERR_INVALID_STATE;

    uint8_t set_coords[] = {
        0x21, 0x00, SSD1306_WIDTH - 1,
        0x22, 0x00, SSD1306_PAGES - 1
    };
    esp_err_t err = ssd1306_send_cmd_list(set_coords, sizeof(set_coords));
    if (err != ESP_OK) return err;

    uint8_t chunk[129];
    chunk[0] = 0x40;

    for (int p = 0; p < SSD1306_PAGES; p++) {
        memcpy(&chunk[1], &s_framebuffer[p * SSD1306_WIDTH], SSD1306_WIDTH);
        err = i2c_master_transmit(s_oled_dev, chunk, sizeof(chunk), 200);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to transmit display page %d: %s", p, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

static void fb_clear(void) {
    memset(s_framebuffer, 0x00, sizeof(s_framebuffer));
}

static void fb_draw_char(int x, int page, char c) {
    if (page < 0 || page >= SSD1306_PAGES || x < 0 || x > (SSD1306_WIDTH - 6)) {
        return;
    }
    if (c < 32 || c > 126) {
        c = '?';
    }
    const uint8_t *bitmap = s_font5x8[c - 32];
    for (int col = 0; col < 5; col++) {
        s_framebuffer[page * SSD1306_WIDTH + x + col] = bitmap[col];
    }

    s_framebuffer[page * SSD1306_WIDTH + x + 5] = 0x00;
}

static void fb_draw_string(int x, int page, const char *str) {
    if (!str) return;
    int cur_x = x;
    while (*str && cur_x <= (SSD1306_WIDTH - 6)) {
        fb_draw_char(cur_x, page, *str);
        cur_x += 6;
        str++;
    }
}

static void display_cleanup_device(void) {
    if (s_oled_dev) {
        i2c_master_bus_rm_device(s_oled_dev);
        s_oled_dev = NULL;
    }
    s_display_ready = false;
}

esp_err_t display_init(void) {

    display_cleanup_device();

    if (!s_i2c_bus) {
        ESP_LOGI(TAG, "Creating I2C master bus (SDA: GPIO%d, SCL: GPIO%d)...", I2C_SDA_GPIO, I2C_SCL_GPIO);
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_PORT_NUM,
            .sda_io_num = I2C_SDA_GPIO,
            .scl_io_num = I2C_SCL_GPIO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };

        esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(err));
            return err;
        }
    }

    uint16_t found_addr = 0;
    esp_err_t probe_err = i2c_master_probe(s_i2c_bus, SSD1306_I2C_ADDR_DEF, I2C_PROBE_TIMEOUT_MS);
    if (probe_err == ESP_OK) {
        found_addr = SSD1306_I2C_ADDR_DEF;
        ESP_LOGI(TAG, "SSD1306 ACK confirmed at 0x%02X", found_addr);
    } else {
        probe_err = i2c_master_probe(s_i2c_bus, SSD1306_I2C_ADDR_ALT, I2C_PROBE_TIMEOUT_MS);
        if (probe_err == ESP_OK) {
            found_addr = SSD1306_I2C_ADDR_ALT;
            ESP_LOGI(TAG, "SSD1306 ACK confirmed at 0x%02X", found_addr);
        }
    }

    if (found_addr == 0) {
        ESP_LOGW(TAG, "SSD1306 display not detected on I2C bus (neither 0x3C nor 0x3D responded with ACK).");
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = found_addr,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(s_i2c_bus, &dev_config, &s_oled_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OLED device on I2C bus: %s", esp_err_to_name(err));
        return err;
    }

    const uint8_t init_cmds[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0xCF,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0xAF
    };

    err = ssd1306_send_cmd_list(init_cmds, sizeof(init_cmds));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send SSD1306 init commands: %s. Cleaning up.", esp_err_to_name(err));
        display_cleanup_device();
        return err;
    }

    fb_clear();
    err = ssd1306_update_screen();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed initial framebuffer flush: %s. Cleaning up.", esp_err_to_name(err));
        display_cleanup_device();
        return err;
    }

    s_display_ready = true;
    ESP_LOGI(TAG, "SSD1306 128x64 OLED initialized and ready at 0x%02X.", found_addr);
    return ESP_OK;
}

bool display_is_available(void) {
    return s_display_ready;
}

esp_err_t display_clear(void) {
    if (!s_display_ready) return ESP_ERR_INVALID_STATE;
    fb_clear();
    return ssd1306_update_screen();
}

esp_err_t display_show_measurement(const measurement *m, const char *status_msg) {
    if (!s_display_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!m) {
        return ESP_ERR_INVALID_ARG;
    }

    fb_clear();

    fb_draw_string(0, 0, "=== SO1L SENSOR ===");

    char line[32];
    if (m->temperature_valid) {
        snprintf(line, sizeof(line), "Temp:  %.1f C", m->temperature_c);
    } else {
        snprintf(line, sizeof(line), "Temp:  [SENSOR ERR]");
    }
    fb_draw_string(0, 2, line);

    if (m->moisture_raw_valid) {
        snprintf(line, sizeof(line), "Raw:   %d counts", m->moisture_raw);
    } else {
        snprintf(line, sizeof(line), "Raw:   [ADC ERR]");
    }
    fb_draw_string(0, 3, line);

    if (m->moisture_percent_valid) {
        snprintf(line, sizeof(line), "Moist: %.1f %%", m->moisture_percent);
    } else {
        snprintf(line, sizeof(line), "Moist: [UNCALIBRATED]");
    }
    fb_draw_string(0, 4, line);

    snprintf(line, sizeof(line), "#%-5" PRIu32 " Up: %llus", m->sequence, (unsigned long long)(m->uptime_ms / 1000ULL));
    fb_draw_string(0, 6, line);

    if (status_msg) {
        snprintf(line, sizeof(line), "Stat:  %s", status_msg);
    } else {
        snprintf(line, sizeof(line), "Stat:  RUNNING");
    }
    fb_draw_string(0, 7, line);

    esp_err_t err = ssd1306_update_screen();
    if (err != ESP_OK) {

        ESP_LOGW(TAG, "Display write failure (%s); disabling display driver cleanly.", esp_err_to_name(err));
        display_cleanup_device();
    }
    return err;
}
