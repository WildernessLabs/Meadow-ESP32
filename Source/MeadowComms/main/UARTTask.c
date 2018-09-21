//
//  Implementation of the UART code for Meadow.
//
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "stdlib.h"

#include "UARTTask.h"
#include "Messaging.h"
#include "WiFiTask.h"
#include "BluetoothTask.h"
#include "MeshNetworkTask.h"
#include "Encoders.h"
#include "ErrorCodes.h"

/// <summary>
///     Handle for the UART receive task.
/// </summary>
TaskHandle_t xUARTReceiveTaskHandle = NULL;

/// <summary>
///     Handle for the UART transmit task.
/// </summary>
TaskHandle_t xUARTTransmitTaskHandle = NULL;

/// <summary>
///     Handle for the incomng messages on the UART.
/// </summary>
QueueHandle_t UARTRxQueueHandle = NULL;

/// <summary>
///     Handle for the transmit handle for the UART.
/// </summary>
/// <remarks>
///     The UART transmit queue is used to hold data from tasks ready to
///     send the data to the UART.
/// </remarks>
QueueHandle_t UARTTransmitQueueHandle = NULL;

/// <summary>
///     Name of the UART receive task (used for logging).
/// </summary>
char *UART_RX_TASK_NAME = "UART_RX_TASK";

/// <summary>
///     Name of the UART transmit task (used for logging).
/// </summary>
char *UART_TX_TASK_NAME = "UART_TX_TASK";

static int bytesRemaining = 0;

static uint8_t *nextPayloadLocation;

static struct TaskMessage messageHeader;

/// <summary>
///     Configure the UART and install the driver.
///
///     UART should be configured as 115200 baud, no partity, one stop bit, 8 data bits.
/// </summary>
void SetupUART()
{
    const uart_config_t uart_config =
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(STM32_UART_NUMBER, &uart_config);
    uart_set_pin(STM32_UART_NUMBER, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(STM32_UART_NUMBER, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, PATTERN_QUEUE_LENGTH, &UARTRxQueueHandle, 0);
    //
    //  Set up the pattern matching algorithm.
    //
//    uart_enable_pattern_det_intr(STM32_UART_NUMBER, PATTERN_CHARACTER, NUMBER_OF_PATTERN_CHARACTERS, 10000, 10, 10);
//    uart_pattern_queue_reset(STM32_UART_NUMBER, PATTERN_QUEUE_LENGTH);
}

/// <summary>
///     UART transmit task.
/// </summary>
void UARTTxTask(void *pvParameters)
{
    esp_log_level_set(UART_TX_TASK_NAME, ESP_LOG_INFO);
    while (true)
    {
        struct Response message;
        BaseType_t result = xQueueReceive(UARTTransmitQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            uint8_t buffer[RM_HEADER_LENGTH];

            memset(buffer, 0, RM_HEADER_LENGTH);
            buffer[RM_INTERFACE] = message.interface;
            EncodeUInt32(message.code, buffer + RM_RESPONSE_CODE_BYTE0);
            EncodeUInt32(message.messageID, buffer + RM_MESSAGE_ID_BYTE0);
            uint8_t crc = CRC8(buffer, RM_HEADER_LENGTH);
            buffer[RM_HEADER_CRC] = crc;
            const int txBytes = uart_write_bytes(STM32_UART_NUMBER, (const char *) buffer, RM_HEADER_LENGTH);
            ESP_LOGI(UART_TX_TASK_NAME, "Wrote %d bytes to UART", txBytes);
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xUARTTransmitTaskHandle);
}

/// <summary>
///     Add the message to the appropriate queue if possible.
/// </summary>
uint32_t QueueMessage()
{
    char *taskName = NULL;
    QueueHandle_t queueHandle = NULL;

    switch (messageHeader.interface)
    {
        case INTERFACE_WIFI:
            taskName = WIFI_TASK_NAME;
            queueHandle = WiFiQueueHandle;
            break;
        case INTERFACE_BLUETOOTH:
            taskName = BLUETOOTH_TASK_NAME;
            queueHandle = BluetoothQueueHandle;
            break;
        case INTERFACE_MESH_NETWORK:
            taskName = MESH_NETWORK_TASK_NAME;
            queueHandle = MeshNetworkQueueHandle;
            break;
    }
    if (messageHeader.interface != INTERFACE_SYSTEM)
    {
        if (xQueueSendToBack(queueHandle, &messageHeader, 0) != pdTRUE)
        {
            if (messageHeader.payload != NULL)
            {
                vPortFree(messageHeader.payload);
            }
            return (ERR_QUEUE_ERROR);
        }
        ESP_LOGI(taskName, "Message queued.");
    }
    else
    {
        ESP_LOGI("SYSTEM", "System tasks not implemented.");
    }
    return (ERR_OK);
}

/// <summary>
///     
/// </summary>
void QueueUARTResponse(uint8_t interface, uint32_t code, uint32_t messageID, uint8_t *payload, uint8_t payloadLength)
{
    struct Response response;

    response.interface = interface;
    response.code = code;
    response.messageID = messageID;
    response.payload = payload;
    response.payloadLength = payloadLength;
    if (xQueueSendToBack(UARTTransmitQueueHandle, &response, 0) != pdTRUE)
    {
        ESP_LOGI(UART_TX_TASK_NAME, "Error sending response, cannot queue message");
    }
}

/// <summary>
///     
/// </summary>
void InvalidFrame(uint8_t frameType)
{
    ESP_LOGI(UART_RX_TASK_NAME, "Invalid frame detected.");
}

/// <summary>
///     Reset the message header and delete the message body storage.
/// <summary>
void ResetIncomingMessage(bool releasePayloadMemory)
{
    messageHeader.interface = 0;
    messageHeader.function = 0;
    messageHeader.messageID = 0;
    if ((messageHeader.payload != NULL) && releasePayloadMemory)
    {
        vPortFree(messageHeader.payload);
    }
    messageHeader.payload = NULL;
    messageHeader.payloadLength = 0;
    bytesRemaining  = 0;
    nextPayloadLocation = NULL;
}

/// <summary>
///     Send a positive or negative acknowledgement along with a response code.
/// </summary>
void AcknowledgeFrame(uint8_t ackNak, uint32_t status)
{
}

/// <summary>
///     Process an application header packet.
/// </summary>
/// <remarks>
///     Put packet definition here.
/// </remarks>
void ProcessHeader(uint8_t *packet, uint8_t packetLength)
{
    if (packetLength != TM_HEADER_LENGTH)
    {
        ESP_LOGI(UART_RX_TASK_NAME, "Invalid header legth: %d", packetLength);
        AcknowledgeFrame(MSG_NAK, ERR_INVALID_HEADER);
    }
    else
    {
        ResetIncomingMessage(true);
        messageHeader.interface = packet[0];
        messageHeader.function = ExtractUInt32(packet + TM_FUNCTION_BYTE0);
        messageHeader.messageID = ExtractUInt32(packet + TM_MESSAGE_ID_BYTE0);
        bytesRemaining = ExtractUInt16(packet + TM_PAYLOAD_LENGTH_BYTE0);
        messageHeader.payloadLength = bytesRemaining;
        if (messageHeader.payloadLength > 0)
        {
            messageHeader.payload = pvPortMalloc(messageHeader.payloadLength);
        }
        else
        {
            messageHeader.payload = NULL;
        }
        nextPayloadLocation = messageHeader.payload;
        AcknowledgeFrame(MSG_ACK, ERR_OK);
    }
}
/// <summary>
///     Process an application data packet.
/// </summary>
void ProcessData(uint8_t *packet, uint8_t packetLength)
{
    if ((nextPayloadLocation != NULL) && (bytesRemaining > 0) && (packetLength <= bytesRemaining))
    {
        memcpy(nextPayloadLocation, packet, packetLength);
        nextPayloadLocation += packetLength;
        bytesRemaining -= packetLength;
        if (bytesRemaining == 0)
        {
            QueueMessage();
            ResetIncomingMessage(false);
            ESP_LOGI(UART_RX_TASK_NAME, "Message received and queued.");
        }
        AcknowledgeFrame(MSG_ACK, ERR_OK);
    }
    else
    {
        ESP_LOGI(UART_RX_TASK_NAME, "Unexpected message data bytes (%d received, %d expected)", packetLength, bytesRemaining);
        AcknowledgeFrame(MSG_NAK, ERR_UNEXPECTED_DATA);
    }
}

/// <summary>
///     
/// </summary>
void ValidFrame(uint8_t frameType, uint8_t *packet, uint8_t packetLength)
{
    ESP_LOGI(UART_RX_TASK_NAME, "Valid frame detected.");
    ESP_LOG_BUFFER_HEXDUMP(UART_RX_TASK_NAME, packet, packetLength, ESP_LOG_INFO);
    if (frameType & PKT_TRANSPORT)
    {
        switch (frameType & 0x2f)
        {
            case TRANSPORT_ACK:
                //
                //  Delete any packets waiting to be sent.
                //
                break;
            case TRANSPORT_NAK:
                //
                //  Resend the message.
                //
                break;
            case TRANSPORT_RESET:
                //
                //  Reset the interface back to power on.
                //
                ResetIncomingMessage(true);
                break;
        }
    }
    else
    {
        //
        //  Application data packets are either header or data packets.
        //
        if (frameType & DATA_HEADER)
        {
            ProcessHeader(packet, packetLength);
        }
        else
        {
            ProcessData(packet, packetLength);
        }
    }
}

/// <summary>
///     UART data received interrupt task.
/// </summary>
void UARTRxTask(void *pvParameters)
{
    uart_event_t event;
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE);

    static enum CommunicationStates communicationState = StartOfFrame;
    static int startOfFrameByteCount = 0;
    static uint8_t frameType;
    static uint8_t packetLength = 0;
    static uint8_t *packet = NULL;
    static uint8_t packetIndex = 0;
    static uint32_t checksum = 0;
    static uint32_t calculatedChecksum;
    static int crcByteNumber = 0;
    
    while (true)
    {
        if (xQueueReceive(UARTRxQueueHandle, (void *) &event, portMAX_DELAY))
        {
            memset(data, 0, RX_BUF_SIZE);
            switch (event.type)
            {
                case UART_DATA :
                    uart_read_bytes(STM32_UART_NUMBER, data, event.size, portMAX_DELAY);
                    ESP_LOGI(UART_RX_TASK_NAME, "Data received:");
                    ESP_LOG_BUFFER_HEXDUMP(UART_RX_TASK_NAME, data, event.size, ESP_LOG_INFO);
                    for (int index = 0; index < event.size; index++)
                    {
                        uint8_t byte = data[index];

                        if (startOfFrameByteCount == 2)
                        {
                            startOfFrameByteCount = 0;
                            switch (byte)
                            {
                                case START_OF_FRAME_CHARACTER:
                                    communicationState = FrameType;
                                    break;
                                case FILLER_CHARACTER:
                                    break;
                                default:
                                    communicationState = StartOfFrame;
                                    break;
                            }
                        }
                        else
                        {
                            if (byte == START_OF_FRAME_CHARACTER)
                            {
                                startOfFrameByteCount++;
                            }
                            else
                            {
                                startOfFrameByteCount = 0;
                            }
                            switch (communicationState)
                            {
                                case StartOfFrame:
                                    //
                                    //  We don't need to do anything here as Start of Frame
                                    //  is dealt with above.
                                    //
                                    break;
                                case FrameType:
                                    frameType = byte;
                                    communicationState = Length;
                                    calculatedChecksum = CRC32_SEED;
                                    calculatedChecksum = ProgressiveCRC32(calculatedChecksum, byte);
                                    break;
                                case Length:
                                    packetLength = byte;
                                    calculatedChecksum = ProgressiveCRC32(calculatedChecksum, byte);
                                    if (packetLength > 0)
                                    {
                                        packet = pvPortMalloc(packetLength);
                                        packetIndex = 0;
                                        communicationState = Payload;
                                    }
                                    else
                                    {
                                        communicationState = CRC;
                                        crcByteNumber = 0;
                                    }
                                    break;
                                case Payload:
                                    packet[packetIndex] = byte;
                                    packetIndex++;
                                    calculatedChecksum = ProgressiveCRC32(calculatedChecksum, byte);
                                    if (packetIndex == packetLength)
                                    {
                                        communicationState = CRC;
                                        crcByteNumber = 0;
                                        checksum = 0;
                                    }
                                    break;
                                case CRC:
                                    checksum |= (byte << (crcByteNumber * 8));
                                    crcByteNumber++;
                                    if (crcByteNumber == 4)
                                    {
                                        communicationState = EndOfFrame;                                    
                                    }
                                    break;
                                case EndOfFrame:
                                    if ((byte == END_OF_FRAME_CHARACTER) && (calculatedChecksum == checksum)) 
                                    {
                                        ValidFrame(frameType, packet, packetLength);
                                    }
                                    else
                                    {
                                        InvalidFrame(frameType);
                                        communicationState = StartOfFrame;
                                    }
                                    if (packet != NULL)
                                    {
                                        vPortFree(packet);
                                        packet = NULL;
                                    }
                                    communicationState = StartOfFrame;
                                    break;
                                default:
                                    communicationState = StartOfFrame;
                                    break;
                            }
                        }
                    }
                    break;
                case UART_BUFFER_FULL:
                case UART_FIFO_OVF:
                    //
                    //  Flush the input buffer in the case of a full ring buffer or a FIFO overflow.
                    //
                    uart_flush_input(STM32_UART_NUMBER);
                    xQueueReset(UARTRxQueueHandle);
                    break;
                default:
                    ESP_LOGI(UART_RX_TASK_NAME, "Unable to process UART event type: %d", event.type);
                    uart_flush_input(STM32_UART_NUMBER);
                    xQueueReset(UARTRxQueueHandle);
                    xQueueReset(UARTTransmitQueueHandle);
                    break;
            }
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    free(data);
    data = NULL;
    if (packet != NULL)
    {
        vPortFree(packet);
        packet = NULL;
    }
    vTaskDelete(xUARTReceiveTaskHandle);
}
