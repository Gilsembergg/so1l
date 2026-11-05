#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include "esp_err.h"
#include "measurement.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_init(void);

bool display_is_available(void);

esp_err_t display_show_measurement(const measurement *m, const char *status_msg);

esp_err_t display_clear(void);

#ifdef __cplusplus
}
#endif

#endif

