#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "moisture.h"
#include "temperature.h"
#include "timestamp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MEASUREMENT_OK              = 0,
    MEASUREMENT_ERR_TEMP_COMM   = (1 << 0),
    MEASUREMENT_ERR_TEMP_CRC    = (1 << 1),
    MEASUREMENT_ERR_MOIST_ADC   = (1 << 2),
    MEASUREMENT_ERR_MOIST_UNCAL = (1 << 3),
    MEASUREMENT_ERR_TIME_UNSYNC = (1 << 4),
} measurement_error_t;

typedef struct {
    uint32_t sequence;
    uint64_t uptime_ms;

    float temperature_c;
    bool temperature_valid;
    int temperature_error_code;

    int moisture_raw;
    bool moisture_raw_valid;
    float moisture_percent;
    bool moisture_percent_valid;

    timestamp_info_t time_info;

    uint32_t error_flags;
} measurement;

esp_err_t measurement_subsystem_init(void);

measurement measurement_read(void);

#ifdef __cplusplus
}
#endif

#endif
