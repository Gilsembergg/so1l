#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include "esp_err.h"
#include "measurement.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t network_init(void);

bool network_is_ready(void);

esp_err_t network_publish_measurement(const measurement *m);

const char *network_get_status_str(void);

#ifdef __cplusplus
}
#endif

#endif

