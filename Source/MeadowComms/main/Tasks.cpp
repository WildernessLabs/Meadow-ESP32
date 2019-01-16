/*
 *  Tasks.cpp
 *
 *	Create the tasks for the RTOS.
 */

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "Tasks.hpp"

#include  "Messaging.h"

/// <summary>
///     Handle for the Bluetooth processing queue.
/// </summary>
QueueHandle_t BluetoothQueueHandle = NULL;

/// <summary>
///     Handle for the Bluetooth task.
/// </summary>
TaskHandle_t xBluetoothTaskHandle = NULL;

/// <summary>
///     Name of the Bluetooth task (used for logging).
/// </summary>
char *BLUETOOTH_TASK_NAME = const_cast<char *> ("BluetoothTask");

/// <summary>
///     Handle for the Mesh network messages to be processed by the Mesh network task.
/// </summary>
QueueHandle_t MeshNetworkQueueHandle = NULL;

/// <summary>
///     Handle for the Mesh Network task.
/// </summary>
TaskHandle_t xMeshNetworkTaskHandle = NULL;

/// <summary>
///     Name of the Mesh network task (used for logging).
/// </summary>
char *MESH_NETWORK_TASK_NAME = const_cast<char *> ("MeshNetworkTask");

/// <summary>
///     Task to process the Bluetooth messages.
/// <summary>
void BluetoothTask(void *pvParameters)
{
    while (true)
    {
        struct ApplicationRequest message;
        BaseType_t result = xQueueReceive(BluetoothQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            ESP_LOGI(BLUETOOTH_TASK_NAME, "Message handling not implemented, message will be deleted");
            //
            //  Memory for the payload is allocated on the heap and it needs to be released.
            //
            if(message.payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(BLUETOOTH_TASK_NAME, message.payload, message.payloadLength, ESP_LOG_INFO);
                vPortFree(message.payload);
            }
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xBluetoothTaskHandle);
}


/// <summary>
///     Task to process the Mesh network messages.
/// <summary>
void MeshNetworkTask(void *pvParameters)
{
    while (true)
    {
        struct ApplicationRequest message;
        BaseType_t result = xQueueReceive(MeshNetworkQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            ESP_LOGI(MESH_NETWORK_TASK_NAME, "Message handling not implemented, message will be deleted");
            //
            //  Memory for the payload is allocated on the heap and it needs to be released.
            //
            if(message.payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(MESH_NETWORK_TASK_NAME, message.payload, message.payloadLength, ESP_LOG_INFO);
                vPortFree(message.payload);
            }
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xMeshNetworkTaskHandle);
}
