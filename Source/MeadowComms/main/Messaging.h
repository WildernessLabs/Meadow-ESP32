#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdlib.h"

#define TM_HEADER_BYTE0     0
#define TM_HEADER_BYTE1     1
#define TM_INTERFACE        2
#define TM_FUNCTION_BYTE0   3
#define TM_FUNCTION_BYTE1   4
#define TM_FUNCTION_BYTE2   5
#define TM_FUNCTION_BYTE3   6
#define TM_CRC              7
#define TM_LENGTH_BYTE0     8
#define TM_LENGTH_BYTE1     9
#define TM_PAYLOAD_START    10

#define FUNC_WIFI           0
#define FUNC_BLUETOOTH      1
#define FUNC_MESH_NETWORK   2
#define FUNC_SYSTEM         3

#define MAX_QUEUE_LENGTH    5

/// <summary>
///     Handle for the transmit handle for the UART.
/// </summary>
/// <remarks>
///     The UART transmit queue is used to hold data from tasks ready to
///     send the data to the UART.
/// </remarks>
extern QueueHandle_t UARTTransmitQueueHandle;

/// <summary>
///     Handle for the WiFi messages to be processed by the WIFi task.
/// </summary>
extern QueueHandle_t WiFiQueueHandle;

/// <summary>
///     Handle for the Bluetooth messages to be processed by the WiFi task.
/// </summary>
extern QueueHandle_t BluetoothQueueHandle;

/// <summary>
///     Handle for the Mesh network messages to be processed by the Mesh network task.
/// </summary>
extern QueueHandle_t MeshNetworkQueueHandle;

/// <summary>
///     Set up the message queues.
/// </summary>
extern void SetupMessageQueues();

/// <summary>
///     
/// </summary>
struct TaskMessage
{
    uint8_t interface;
    uint32_t function;
    uint16_t payloadLength;
    uint8_t *payload;
};

extern uint8_t CRC8(const uint8_t *, uint16_t);
