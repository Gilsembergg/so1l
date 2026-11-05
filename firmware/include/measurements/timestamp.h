#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t uptime_ms;
    time_t utc_sec;
    struct tm utc_tm;
    bool utc_valid;
} timestamp_info_t;

uint64_t timestamp_get_monotonic_ms(void);

timestamp_info_t timestamp_now(void);

const char *timestamp_format_iso8601(const timestamp_info_t *ts, char *buf, size_t size);

esp_err_t timestamp_sntp_init(void);

void timestamp_sntp_deinit(void);

void timestamp_set_utc_synchronized(bool synced);

bool timestamp_is_utc_synchronized(void);

#ifdef __cplusplus
}
#endif

#endif
