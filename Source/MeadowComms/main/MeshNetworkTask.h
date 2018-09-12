#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

/// <summary>
///     Handle for the Mesh netowrk messages to be processed by the Mesh network task.
/// </summary>
extern QueueHandle_t MeshNetworkQueueHandle;

/// <summary>
///     Handle for the Mesh Network task.
/// </summary>
extern TaskHandle_t xMeshNetworkTaskHandle;

/// <summary>
///     Name of the Mesh network task (used for logging).
/// </summary>
extern char *MESH_NETWORK_TASK_NAME;

/// <summary>
///     Task to process the Mesh network messages.
/// </summary>
extern void MeshNetworkTask(void *);