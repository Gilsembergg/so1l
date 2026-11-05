#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STATUS_UNINITIALIZED,
    WIFI_STATUS_STANDALONE,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_COOLDOWN
} wifi_status_t;

esp_err_t wifi_manager_init(void);

esp_err_t wifi_manager_start(const char *ssid, const char *password);

esp_err_t wifi_manager_stop(void);

bool wifi_manager_is_connected(void);

wifi_status_t wifi_manager_get_status(void);

const char *wifi_manager_get_status_str(void);

#ifdef __cplusplus
}
#endif

#endif
