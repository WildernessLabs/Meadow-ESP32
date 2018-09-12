#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "MeshNetworkTask.h"
#include  "Messaging.h"

/// <summary>
///     Handle for the Mesh netowrk messages to be processed by the Mesh network task.
/// </summary>
QueueHandle_t MeshNetworkQueueHandle = NULL;

/// <summary>
///     Handle for the Mesh Network task.
/// </summary>
TaskHandle_t xMeshNetworkTaskHandle = NULL;

/// <summary>
///     Name of the Mesh network task (used for logging).
/// </summary>
char *MESH_NETWORK_TASK_NAME = "MESH_NETWORK_TASK_NAME";

/// <summary>
///     Task to process the Mesh network messages.
/// <summary>
void MeshNetworkTask(void *pvParameters)
{
    esp_log_level_set(MESH_NETWORK_TASK_NAME, ESP_LOG_INFO);
    ESP_LOGI(MESH_NETWORK_TASK_NAME, "Mesh network task created.");
    while (true)
    {
        struct TaskMessage message;
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