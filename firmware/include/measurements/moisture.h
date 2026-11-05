#ifndef MOISTURE_H
#define MOISTURE_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int raw;
    bool raw_valid;
    float percent;
    bool percent_valid;
} moisture_reading_t;

esp_err_t moisture_init(void);

moisture_reading_t moisture_read(void);

esp_err_t moisture_set_calibration(int raw_dry, int raw_wet);

bool moisture_is_calibrated(void);

#ifdef __cplusplus
}
#endif

#endif
