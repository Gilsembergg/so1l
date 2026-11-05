#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t temperature_init(void);

esp_err_t temperature_read(float *out_celsius);

#ifdef __cplusplus
}
#endif

#endif
// NOTE: insert here prefered temperature format ( Kelvin, Celsius, Fahrenheit ) and conversion functions if needed.