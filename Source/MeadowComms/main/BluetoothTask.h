#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/// <summary>
///     Handle for the Bluetooth processing queue.
/// </summary>
extern QueueHandle_t BluetoothQueueHandle;

/// <summary>
///     Handle for the Bluetooth task.
/// </summary>
extern TaskHandle_t xBluetoothTaskHandle;

/// <summary>
///     Name of the Bluetooth task (used for logging).
/// </summary>
extern char *BLUETOOTH_TASK_NAME;

/// <summary>
///     Task to process the Bluetooth messages.
/// <summary>
void BluetoothTask(void *);
