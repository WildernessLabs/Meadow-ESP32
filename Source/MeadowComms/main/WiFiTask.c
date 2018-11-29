#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"

#include "WiFiTask.h"
#include "Messaging.h"
#include "UARTTask.h"

/// <summary>
///     Handle for the WiFi messages to be processed by the WIFi task.
/// </summary>
QueueHandle_t WiFiQueueHandle = NULL;

/// <summary>
///     Handle for the WiFi task.
/// </summary>
TaskHandle_t xWiFiTaskHandle = NULL;

/// <summary>
///     Name of the WiFi task (used for logging).
/// </summary>
char *WIFI_TASK_NAME = "WIFI_TASK";

/// <summary>
///     Task to process the WiFi messages.
/// <summary>
void WiFiTask(void *pvParameters)
{
    while (true)
    {
        struct ApplicationRequest message;
        BaseType_t result = xQueueReceive(WiFiQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            ESP_LOGI(WIFI_TASK_NAME, "WiFi message received, function %d", message.function);
            //
            //  Memory for the payload is allocated on the heap and it needs to be released.
            //
            if (message.payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(WIFI_TASK_NAME, message.payload, message.payloadLength, ESP_LOG_INFO);
            }
        }
        QueueUARTResponse(message.interface, 10, message.messageID, message.payload, message.payloadLength);
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xWiFiTaskHandle);
}