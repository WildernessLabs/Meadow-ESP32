#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "UARTTask.h"
#include "Messaging.h"
#include "BluetoothTask.h"
#include "WiFiTask.h"
#include "MeshNetworkTask.h"

/// <summary>
///     Handle for the transmit handle for the UART.
/// </summary>
/// <remarks>
///     The UART transmit queue is used to hold data from tasks ready to
///     send the data to the UART.
/// </remarks>
QueueHandle_t UARTTransmitQueueHandle = NULL;

/// <summary>
///     Handle for the WiFi messages to be processed by the WIFi task.
/// </summary>
QueueHandle_t WiFiQueueHandle;

/// <summary>
///     Handle for the Bluetooth messages to be processed by the WiFi task.
/// </summary>
QueueHandle_t BluetoothQueueHandle;

/// <summary>
///     Handle for the Mesh network messages to be processed by the Mesh network task.
/// </summary>
QueueHandle_t MeshNetworkQueueHandle;

/// <summary>
///     Set up the message queues.
/// </summary>
void SetupMessageQueues()
{
    const size_t messageSize = sizeof(struct TaskMessage);
    UARTTransmitQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (UARTTransmitQueueHandle == NULL)
    {
        ESP_LOGI(UART_TX_TASK_NAME, "Cannot allocate UART transmit queue.");
    }
    else
    {
        ESP_LOGI(UART_TX_TASK_NAME, "UART transmit queue created.");
    }
    //
    WiFiQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (WiFiQueueHandle == NULL)
    {
        ESP_LOGI(WIFI_TASK_NAME, "Cannot allocate WiFi processing queue.");
    }
    else
    {
        ESP_LOGI(WIFI_TASK_NAME, "WiFi processing queue created.");
    }
    //
    BluetoothQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (BluetoothQueueHandle == NULL)
    {
        ESP_LOGI(BLUETOOTH_TASK_NAME, "Cannot allocate Bluetooth processing queue.");
    }
    else
    {
        ESP_LOGI(BLUETOOTH_TASK_NAME, "Bluetooth processing queue created.");
    }
    //
    MeshNetworkQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (MeshNetworkQueueHandle == NULL)
    {
        ESP_LOGI(MESH_NETWORK_TASK_NAME, "Cannot allocate Mesh network queue.");
    }
    else
    {
        ESP_LOGI(MESH_NETWORK_TASK_NAME, "Mesh network queue created.");
    }
}

/// <summary>
///     Calculate the CRC value for the specified data buffer.
/// </summary>
/// <remarks>
///     This algorithm is loosly based upon the Dallas 1-Wire algotithm.
/// </remarks>
uint8_t CRC8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        uint8_t byte = data[byteCounter];
        for (uint8_t bitCounter = 8; bitCounter; bitCounter--) 
        {
            uint8_t sum = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (sum)
            {
                crc ^= 0x8C;
            }
            byte >>= 1;
        }
    }
    return(crc);
}