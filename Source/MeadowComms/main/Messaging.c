#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "UARTTask.h"
#include "Messaging.h"
#include "WiFiTask.h"
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
    xWiFiQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (xWiFiQueueHandle == NULL)
    {
        ESP_LOGI(__func__, "Cannot allocate WiFi processing queue.");
    }
    //
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
