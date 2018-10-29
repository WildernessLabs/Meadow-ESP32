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
///     Handle for the incoming messages on the UART.
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

static struct ApplicationRequest messageHeader;

/// <summary>
///     Configure the UART and install the driver.
///
///     UART should be configured as 115200 baud, no partity, one stop bit, 8 data bits.
/// </summary>
void SetupUART()
{
    const uart_config_t uart_config =
    {
        .baud_rate = 57600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(STM32_UART_NUMBER, &uart_config);
    uart_set_pin(STM32_UART_NUMBER, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(STM32_UART_NUMBER, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, UART_QUEUE_LENGTH, &UARTRxQueueHandle, 0);
}

/// <summary>
///     UART transmit task.
/// </summary>
void UARTTxTask(void *pvParameters)
{
    esp_log_level_set(UART_TX_TASK_NAME, ESP_LOG_INFO);
    while (true)
    {
        struct ApplicationResponse message;
        BaseType_t result = xQueueReceive(UARTTransmitQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            uint8_t buffer[APP_RESPONSE_HEADER_LENGTH];

            memset(buffer, 0, APP_RESPONSE_HEADER_LENGTH);
            buffer[APP_RESPONSE_INTERFACE] = message.interface;
            EncodeUInt32(message.code, buffer + APP_RESPONSE_CODE);
            EncodeUInt32(message.messageID, buffer + APP_RESPONSE_MESSAGE_ID);
            EncodeUInt32(message.payloadLength, buffer + APP_RESPONSE_PAYLOAD_LENGTH);
            const int txBytes = uart_write_bytes(STM32_UART_NUMBER, (const char *) buffer, APP_RESPONSE_HEADER_LENGTH);
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
    struct ApplicationResponse response;

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
///     Work out how big the frame buffer should be for the message that
//      is going to be sent.  This will take into account the padding
//      and CRC etc.
/// </summary>
uint32_t HeaderAndPayloadSize(uint8_t *buffer, uint8_t bufferLength)
{
    uint32_t frameLength = 0;

    int startOfFrameCharacterCount = 0;
    int byteCount = 0;
            
    for (int index = 0; index < bufferLength; index++)
    {
        if (buffer[index] == START_OF_FRAME_CHARACTER)
        {
            startOfFrameCharacterCount++;
            if (startOfFrameCharacterCount == (NUMBER_OF_START_OF_FRAME_CHARACTERS - 1))
            {
                byteCount++;     // An additional byte is needed for the padding character.
                startOfFrameCharacterCount = 0;
            }
        }
        else
        {
            startOfFrameCharacterCount = 0;
        }
        byteCount++;
    }
    byteCount += bufferLength;       //  Size of the header.
    byteCount += 4;                  //  CRC is a 32-bit CRC and takes 4 bytes.
    return(byteCount);
}

/// <summary>
///     Take a buffer of data and generate a sequences of bytes to send over the UART.
//      This sequence will contain the start of frame characters, data, CRC and end
//      of frame characters.
/// </summary>
uint8_t *GenerateFrame(uint8_t *buffer, uint32_t bufferLength, uint32_t frameLength)
{
    uint8_t *frame = pvPortMalloc(frameLength);
    memset(frame, START_OF_FRAME_CHARACTER, NUMBER_OF_START_OF_FRAME_CHARACTERS);
    uint32_t currentByte = NUMBER_OF_START_OF_FRAME_CHARACTERS;
    //
    //  Copy the data into the new frame taking into account the need to insert
    //  padding characters if we hit a sequence of start of frame characters.
    //
    int startOfFrameCharacterCount = 0;
    for (int index = 0; index < bufferLength; index++)
    {
        frame[currentByte++] = buffer[index];
        if (buffer[index] == START_OF_FRAME_CHARACTER)
        {
            startOfFrameCharacterCount++;
            if (startOfFrameCharacterCount == (NUMBER_OF_START_OF_FRAME_CHARACTERS - 1))
            {
                frame[currentByte++] = PADDING_CHARACTER;
                startOfFrameCharacterCount = 0;
            }
        }
        else
        {
            startOfFrameCharacterCount = 0;
        }
    }
    EncodeUInt32(CRC32(frame,  frameLength - 4), frame + currentByte);
    currentByte += 4;
    frame[currentByte] = END_OF_FRAME_CHARACTER;
    return (frame);
}

/// <summary>
///     Send a positive (ACK) or negative (NAK) acknowledgement along with a response code.
/// </summary>
void AcknowledgeFrame(uint8_t ackNak, uint32_t status)
{
    uint8_t buffer[TRANSPORT_MESSAGE_LENGTH];
    memset(buffer, 0, TRANSPORT_MESSAGE_LENGTH);
    buffer[TRANSPORT_FRAME_TYPE] = PKT_TRANSPORT | ackNak;
    EncodeUInt32(status, buffer + TRANSPORT_STATUS_CODE);
    uint32_t frameLength = HeaderAndPayloadSize(buffer, TRANSPORT_MESSAGE_LENGTH);
    uint8_t *frame = GenerateFrame(buffer, TRANSPORT_MESSAGE_LENGTH, frameLength);
    //
    //  TODO: Need to work out how to lock the UART.
    //
    const int txBytes = uart_write_bytes(STM32_UART_NUMBER, (const char *) frame, frameLength);
    vPortFree(frame);
    ESP_LOGI(UART_TX_TASK_NAME, "Wrote %d bytes to UART", txBytes);
}

/// <summary>
///     Process an application header packet.
/// </summary>
/// <remarks>
///     Put packet definition here.
/// </remarks>
void ProcessHeader(uint8_t *packet, uint8_t packetLength)
{
    if (packetLength != APP_REQUEST_HEADER_LENGTH)
    {
        ESP_LOGI(UART_RX_TASK_NAME, "Invalid header length: %d", packetLength);
        AcknowledgeFrame(TRANSPORT_NAK, ERR_INVALID_HEADER);
    }
    else
    {
        ResetIncomingMessage(true);
        messageHeader.interface = packet[0];
        messageHeader.function = ExtractUInt32(packet + APP_REQUEST_FUNCTION);
        messageHeader.messageID = ExtractUInt32(packet + APP_REQUEST_MESSAGE_ID);
        bytesRemaining = ExtractUInt16(packet + APP_REQUEST_PAYLOAD_LENGTH);
        messageHeader.payloadLength = bytesRemaining;
        if (messageHeader.payloadLength > 0)
        {
            messageHeader.payload = pvPortMalloc(messageHeader.payloadLength);
        }
        else
        {
            messageHeader.payload = NULL;
            //
            //  Messages with no payload should be queued immediately.
            //
            QueueMessage();
        }
        nextPayloadLocation = messageHeader.payload;
        AcknowledgeFrame(TRANSPORT_ACK, ERR_OK);
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
            //
            //  It is important NOT to release the memory containing the payload
            //  as the pointer to the payload has been copied into the queue for
            //  the appropriate interface.
            //
            //  It is the responsibility of the queue handler to release the payload
            //  memory when the handler has finished processing the message.
            //  Releasing the memory here will corrupt the heap.
            //
            ResetIncomingMessage(false);
            ESP_LOGI(UART_RX_TASK_NAME, "Message received and queued.");
        }
        AcknowledgeFrame(TRANSPORT_ACK, ERR_OK);
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
    static int byteNumber = 0;
    static uint32_t messageID = 0;
    static uint8_t sequenceNumber = 0;
    static uint32_t statusCode = 0;
    
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
                                case PADDING_CHARACTER:
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
                                    if ((frameType & PKT_TRANSPORT) > 0)
                                    {
                                        communicationState = StatusCode;
                                    }
                                    else
                                    {
                                        communicationState = MessageID;
                                    }
                                    calculatedChecksum = CRC32_SEED;
                                    calculatedChecksum = ProgressiveCRC32(calculatedChecksum, byte);
                                    byteNumber = 0;
                                    break;
                                case StatusCode:
                                        statusCode |= (byte << (byteNumber * 8));
                                        byteNumber++;
                                        if (byteNumber == 4)
                                        {
                                            communicationState = CRC;
                                        }
                                        calculatedChecksum = ProgressiveCRC32(calculatedChecksum, byte);
                                    break;
                                case MessageID:
                                    messageID |= (byte << (byteNumber * 8));
                                    byteNumber++;
                                    if (byteNumber == 4)
                                    {
                                        communicationState = SequenceNumber;                                    
                                    }
                                    calculatedChecksum = ProgressiveCRC32(calculatedChecksum, byte);
                                    break;
                                case SequenceNumber:
                                    sequenceNumber = byte;
                                    communicationState = Length;
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
                                        checksum = 0;
                                        byteNumber = 0;
                                    }
                                    break;
                                case Payload:
                                    packet[packetIndex] = byte;
                                    packetIndex++;
                                    calculatedChecksum = ProgressiveCRC32(calculatedChecksum, byte);
                                    if (packetIndex == packetLength)
                                    {
                                        communicationState = CRC;
                                        checksum = 0;
                                        byteNumber = 0;
                                    }
                                    break;
                                case CRC:
                                    checksum |= (byte << (byteNumber * 8));
                                    byteNumber++;
                                    if (byteNumber == 4)
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
