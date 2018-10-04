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
///     Set up the message queues.
/// </summary>
void SetupMessageQueues()
{
    const size_t messageSize = sizeof(struct ApplicationRequest);
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

/// <summary>
///     Calculate the 32-bit CRC of a buffer of data.
/// </summary>
uint32_t CRC32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = CRC32_SEED;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        crc = ProgressiveCRC32(crc, data[byteCounter]);
    }
    return (crc);
}

/// <summary>
///     Calculate the CRC32 value change for the specified byte.
/// </summary>
uint32_t ProgressiveCRC32(uint32_t currentChecksum, uint8_t byte)
{
    uint32_t crc = currentChecksum;
    crc ^= byte;
    for (uint32_t index = 0; index < 8; index++)
    {
        uint32_t mask = (uint32_t) -(crc & 0x01);
        crc = (crc >> 1) ^ (0xedb88320 & mask);
    }
    return (crc);
}