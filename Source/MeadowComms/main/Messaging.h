#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdlib.h"

//
//  Available interfaces.
//
#define INTERFACE_WIFI                  (0)
#define INTERFACE_BLUETOOTH             (1)
#define INTERFACE_MESH_NETWORK          (2)
#define INTERFACE_SYSTEM                (3)
#define INTERFACE_MAX                   (3)

//
//  Length of the queues for the interfaces.
//
#define MAX_QUEUE_LENGTH            (5)

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
extern QueueHandle_t xWiFiQueueHandle;

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

//
//  Bit masks for transport and data packets.
//
#define PKT_DATA                    (0x80)
#define PKT_TRANSPORT               (0x40)

//
//  Transport (UART) packet commands.
//
#define TRANSPORT_ACK               (0x01)
#define TRANSPORT_NAK               (0x02)
#define TRANSPORT_RESET             (0x04)

//
//  Data packet types.
//
#define DATA_HEADER                 (0x01)
#define DATA_PAYLOAD                (0x02)

#define UART_MESSAGE_HEADER_SIZE    (2)

//  
//  This is the format of the data being sent from the STM32 to the ESP32.
//
#define APP_REQUEST_INTERFACE               (0)
#define APP_REQUEST_FUNCTION                (APP_REQUEST_INTERFACE + 1)
#define APP_REQUEST_MESSAGE_ID              (APP_REQUEST_FUNCTION + 4)
#define APP_REQUEST_PAYLOAD_LENGTH          (APP_REQUEST_MESSAGE_ID + 4)
#define APP_REQUEST_HEADER_LENGTH           (APP_REQUEST_PAYLOAD_LENGTH + 4)

/// <summary>
///     Application request from the STM32 for a service.
/// </summary>
struct ApplicationRequest
{
    /// <summary>
    ///     Interface or device that is making the request.
    /// </summary>
    uint8_t interface;

    /// <summary>
    ///     
    /// </summary>
    uint32_t function;

    /// <summary>
    ///     Message ID from the requestor.  This uniquely identifies the 
    ///     request from the STM32.
    /// </summary>
    uint32_t messageID;

    /// <summary>
    ///     Amount of data in the request payload (this does not include this header).
    /// </summary>
    uint32_t payloadLength;

    /// <summary>
    ///     Data supporting the request.
    /// </summary>
    uint8_t *payload;
};

#define APP_RESPONSE_INTERFACE          (0)
#define APP_RESPONSE_CODE               (APP_RESPONSE_INTERFACE + 1)
#define APP_RESPONSE_MESSAGE_ID         (APP_RESPONSE_CODE + 4)
#define APP_RESPONSE_PAYLOAD_LENGTH     (APP_RESPONSE_MESSAGE_ID + 4)
#define APP_RESPONSE_HEADER_LENGTH      (APP_RESPONSE_PAYLOAD_LENGTH + 4)

#define CRC32_SEED                  (0xffffffff)

/// <summary>
///     Structure for holding the elements of a response to a request
///     from the STM32.
/// </summary>
struct ApplicationResponse
{
    /// <summary>
    ///     Interface or device that is responding.
    /// </summary>
    uint8_t interface;

    /// <summary>
    ///     Status code returned.
    /// </summary>
    uint32_t code;

    /// <summary>
    ///     Message ID that was used in the original request.
    /// </summary>
    uint32_t messageID;

    /// <summary>
    ///     Amount of data in the response payload (this does not include this header).
    /// </summary>
    uint32_t payloadLength;

    /// <summary>
    ///     Data to be returned.
    /// </summary>
    uint8_t *payload;
};

#define TRANSPORT_FRAME_TYPE        0
#define TRANSPORT_STATUS_CODE       (TRANSPORT_FRAME_TYPE + 1)
#define TRANSPORT_MESSAGE_LENGTH    (TRANSPORT_STATUS_CODE + 4)

/// <summary>
///     Location of the frame type in the message frame.
/// </summary>
#define FRAME_TYPE                  (0x00)

/// <summary>
///     Location of the message ID in the message frame.
/// </summary>
#define MESSAGE_ID                  (FRAME_TYPE + 1)

/// <summary>
///     Location of the sequence number in the message frame.
/// </summary>
#define SEQUENCE_NUMBER             (MESSAGE_ID + 4)

/// <summary>
///     Location of the frame length in the message frame.
/// </summary>
#define FRAME_PAYLOAD_LENGTH        (SEQUENCE_NUMBER + 1)

/// <summary>
///     Size of the header.
/// </summary>
#define FRAME_HEADER_LENGTH         (FRAME_PAYLOAD_LENGTH + 1)

extern uint8_t CRC8(const uint8_t *, uint16_t);
extern uint32_t CRC32(const uint8_t *, uint16_t);
extern uint32_t ProgressiveCRC32(uint32_t, uint8_t);