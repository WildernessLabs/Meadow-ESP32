#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

//
//  WiFi Function codes
//
#define WIFI_FUNC_START                     0
#define WIFI_FUNC_CONNECT                   1
#define WIFI_FUNC_DISCONNECT                2
#define WIFI_FUNC_GET_ACCESS_POINTS         3
#define WIFI_FUNC_DEEP_SLEEP                4
#define WIFI_FUNC_MAX                       4

struct AccessPointInformation
{
    char        name[33];
    uint8_t     rssi;

};

/// <summary>
///     Handle for the WiFi messages to be processed by the WIFi task.
/// </summary>
extern QueueHandle_t WiFiQueueHandle;

/// <summary>
///     Handle for the WiFi task.
/// </summary>
extern TaskHandle_t xWiFiTaskHandle;

/// <summary>
///     Name of the WiFi task (used for logging).
/// </summary>
extern const char *WIFI_TASK_NAME;

void WiFiTask(void *); 
