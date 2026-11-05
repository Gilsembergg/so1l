#ifndef INDICATORS_H
#define INDICATORS_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INDICATOR_STATE_OFF,
    INDICATOR_STATE_STARTUP,
    INDICATOR_STATE_NORMAL,
    INDICATOR_STATE_DEGRADED,
    INDICATOR_STATE_CRITICAL
} indicator_state_t;

esp_err_t indicators_init(void);

void indicators_set_state(indicator_state_t state);

void indicators_set_raw(bool green, bool yellow, bool red);

void indicators_self_test(void);

#ifdef __cplusplus
}
#endif

#endif

