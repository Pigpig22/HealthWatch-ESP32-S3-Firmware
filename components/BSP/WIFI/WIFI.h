#ifndef _WIFI_H
#define _WIFI_H

#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define WIFI_SSID      CONFIG_HEALTHWATCH_WIFI_SSID
#define WIFI_PASS      CONFIG_HEALTHWATCH_WIFI_PASSWORD
#define MAX_RETRY      10

void wifi_init_sta(void);
bool wifi_is_connected(void);

#endif
