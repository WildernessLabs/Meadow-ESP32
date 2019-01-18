#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "Messaging.h"
#include "Tasks.hpp"

/// <summary>
///     Set up the message queues.
/// </summary>
void SetupMessageQueues()
{
    UARTTransmitQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, sizeof(struct ApplicationResponse));
    if (UARTTransmitQueueHandle == NULL)
    {
        ESP_LOGI(__func__, "Cannot allocate UART transmit queue.");
    }
    //
    const size_t messageSize = sizeof(struct ApplicationRequest);
    BluetoothQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (BluetoothQueueHandle == NULL)
    {
        ESP_LOGI(__func__, "Cannot allocate Bluetooth processing queue.");
    }
    //
    MeshNetworkQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (MeshNetworkQueueHandle == NULL)
    {
        ESP_LOGI(MESH_NETWORK_TASK_NAME, "Cannot allocate Mesh network queue.");
    }
}
