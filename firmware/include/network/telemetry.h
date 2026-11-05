#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "measurement.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SO1L_TELEMETRY_SCHEMA_VERSION "1.0.0"
#define SO1L_DEFAULT_MODULE_ID        "so1l-probe-01"
#define SO1L_TELEMETRY_BUFFER_SIZE    512

typedef struct {
    const char *module_id;
} telemetry_config_t;

esp_err_t telemetry_init(const telemetry_config_t *config);

esp_err_t telemetry_serialize(const measurement *m, char *out_buf, size_t buf_size, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif

