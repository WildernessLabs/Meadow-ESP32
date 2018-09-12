#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

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
extern char *WIFI_TASK_NAME;

void WiFiTask(void *); 