#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include  "BluetoothTask.h"
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
char *BLUETOOTH_TASK_NAME = "BLUETOOTH_TASK";

/// <summary>
///     Task to process the Bluetooth messages.
/// <summary>
void BluetoothTask(void *pvParameters)
{
    esp_log_level_set(BLUETOOTH_TASK_NAME, ESP_LOG_INFO);
    ESP_LOGI(BLUETOOTH_TASK_NAME, "Bluetooth task created.");
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