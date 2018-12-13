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
#include "esp_app_trace.h"
#include "string.h"

#include "UARTTask.h"
#include "Messaging.h"
#include "WiFiTask.h"
#include "BluetoothTask.h"
#include "MeshNetworkTask.h"
#include "Encoders.h"
#include "ErrorCodes.h"

// ---------------------------------------------------------------------------
//
//                          Global variables.
//
// ---------------------------------------------------------------------------

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
///     Handle to the queue used to sync the UART messages.
/// </summary>
/// <remarks>
///     The sync queue is used by the transport message handling methods
///     to communicate with the Tx and Rx tasks to provide a measure of
///     reliability and recovery.
/// </remarks>
QueueHandle_t xUARTSyncQueueHandle = NULL;

/// <summary>
///     This is the maximum number of milliseconds between sending a frame
///     and a response being received by the ESP32.  This is a variable as
///     it will depend upon the baud rate and the size of a frame.
///
///     This value should be set whan the UART os configured.
/// </summary>
TickType_t _uartMaximumSendResponsePeriod = 0;

/// <summary>
///     Mutex to ensure that only one request to access the UART can be 
///     processed at any time.
/// </summary>
/// <remarks>
///     The UARTTXTask and the ACK/NAK processing could need access to 
///     the UART and this mutex is used to synchronise the access.
/// </remarks>
SemaphoreHandle_t xUARTTxMutex = NULL;

static int bytesRemaining = 0;

static uint8_t *nextPayloadLocation;

static struct ApplicationRequest messageHeader;

/// <summary>
///     Holds the current state of the communication state machine.
/// </summary>
static enum CommunicationStates _communicationState = StartOfFrame;

/// <summary>
///     Number of consecutive occurrences of the start of frame character has been detected
/// </summary>
static int _startOfFrameByteCount = 0;

/// <summary>
///     Type of frame being received.
/// </summary>
static uint8_t _currentFrameType;

/// <summary>
///     Number of bytes in the current data frame being received.
/// </summary>
static uint8_t _framePayloadLength = 0;

/// <summary>
///     Payload of the frame currently being received.
/// </summary>
static uint8_t *_framePayload = NULL;

/// <summary>
///     Location of the next payload byte in the buffer.
/// </summary>
static uint8_t _framePayloadIndex = 0;

/// <summary>
///     Value of the checksum as read from the incoming data frame.
/// </summary>
static uint32_t _originalChecksum = 0;

/// <summary>
///     Value of the calculated checksum from the progressive checksum calculation.
/// </summary>
static uint32_t _calculatedChecksum;

/// <summary>
///     This variable holds the byte number when putting together multi-byte
///     values (i.e. 4 bytes in a UInt32 object).  this allows the original
///     object to be re-hydrated from the UART byte stream.
/// </summary>
static int _byteNumber = 0;

/// <summary>
///     Message ID of the data packets being received.
/// </summary>
static uint32_t _messageID = 0;

/// <summary>
///     Sequence number of this frame in the total number of frames being sent.
/// </summary>
static uint8_t _frameSequenceNumber = 0;

/// <summary>
///     Packet code returned by the transport packets.
/// </summary>
static uint32_t _statusCode = 0;

/// <summary>
///     Buffer for the start of frame characters.
/// </summary>
static char _startOfFrameCharacters[NUMBER_OF_START_OF_FRAME_CHARACTERS];

/// <summary>
///     Buffer for the end of frame characters.
/// </summary>
static char _endOfFrameCharacters[NUMBER_OF_END_OF_FRAME_CHARACTERS];

/// <summary>
///     Current retry count.
/// </summary>
static uint32_t _currentRetryCount = 0;

/// <summary>
///     Current maximum number of retries when sending a frame
///     before the system declares an error condition.
/// </summary>
static uint32_t _maximumRetryCount = UART_DEFAULT_MAXIMUM_RETRIES;

/// <summary>
///     Total number of bytes sent since the microcontroller was started.
/// </summary>
static uint _totalTxBytes = 0;

/// <summary>
///     Total number of bytes received since the microcontroller was started.
/// </summary>
static uint _totalRxBytes = 0;

/// <summary>
///     Total number of frames sent since the microcontroller was started.
/// </summary>
static uint _totalFramesSent = 0;

/// <summary>
///     Total number of frames received since the microcontroller was started.
/// </summary>
static uint _totalFramesReceived = 0;

/// <summary>
///     Number of Rx errors since the microcontroller was started.
/// </summary>
static uint _totalRxErrors = 0;

/// <summary>
///     Number of Tx errors since the microcontroller was started.
/// </summary>
static uint _totalTxErrors = 0;

//
//  Buffer to hold messages to be sent to ESP32 Semihosting output.
//
static char _semihostingBuffer[128];

//
//  ESP heap debugging stuff.
//
#include "esp_heap_trace.h"
#define NUMBER_OF_HEAP_TRACE_RECORDS 100
static heap_trace_record_t heap_trace_records[NUMBER_OF_HEAP_TRACE_RECORDS];

// ---------------------------------------------------------------------------
//
//                                  Methods
// 
// ---------------------------------------------------------------------------

/// <summary>
///     Configure the UART and install the driver.
///
///     UART should be configured as 115200 baud, no parity, one stop bit, 8 data bits.
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
    //
    //  TODO: Calculate this value, for debugging we will use an approximate value.
    //
    _uartMaximumSendResponsePeriod = 250 / portTICK_PERIOD_MS;
    //
    uart_param_config(STM32_UART_NUMBER, &uart_config);
    uart_set_pin(STM32_UART_NUMBER, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(STM32_UART_NUMBER, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, UART_QUEUE_LENGTH, &UARTRxQueueHandle, 0);
    xUARTTxMutex = xSemaphoreCreateMutex();
    xUARTSyncQueueHandle = xQueueCreate(1, sizeof(enum UARTSyncStates));
    //    ESP_ERROR_CHECK(heap_trace_init_standalone(heap_trace_records, NUMBER_OF_HEAP_TRACE_RECORDS));
    memset(_startOfFrameCharacters, START_OF_FRAME_CHARACTER, NUMBER_OF_START_OF_FRAME_CHARACTERS);
    memset(_endOfFrameCharacters, END_OF_FRAME_CHARACTER, NUMBER_OF_END_OF_FRAME_CHARACTERS);
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
    QueueHandle_t queueHandle = NULL;

    switch (messageHeader.interface)
    {
        case INTERFACE_WIFI:
            queueHandle = xWiFiQueueHandle;
            break;
        case INTERFACE_BLUETOOTH:
            queueHandle = BluetoothQueueHandle;
            break;
        case INTERFACE_MESH_NETWORK:
            queueHandle = MeshNetworkQueueHandle;
            break;
    }
    if (messageHeader.interface != INTERFACE_SYSTEM)
    {
        if (xQueueSendToBack(queueHandle, &messageHeader, 0) != pdPASS)
        {
            if (messageHeader.payload != NULL)
            {
                vPortFree(messageHeader.payload);
            }
            return (ERR_QUEUE_ERROR);
        }
    }
    else
    {
        ESP_LOGI("SYSTEM", "System tasks not implemented.");
    }
    return (ERR_COMPLETED_OK);
}

/// <summary>
///     Add a response to the UART Tx queue.
/// </summary>
/// <param name="respopnse">Response to be queued for sending to the STM32.</param>
void QueueUARTResponse(struct ApplicationResponse *response)
{
    if (xQueueSendToBack(UARTTransmitQueueHandle, response, 0) != pdPASS)
    {
        ESP_LOGI(__func__, "Error sending response, cannot queue message");
    }
}

/// <summary>
///     Work out how big the frame buffer should be for the message that
//      is going to be sent.  This will take into account the padding
//      and CRC etc.
/// </summary>
uint32_t CalculateEncodedFrameLength(uint8_t *buffer, uint8_t bufferLength)
{
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
    //
    //  We need to take into consideration the size of the buffer required for
    //  the data plus the start of frame characters, the CRC and the end of 
    //  frame characters.
    //
    byteCount += (NUMBER_OF_START_OF_FRAME_CHARACTERS + NUMBER_OF_END_OF_FRAME_CHARACTERS + 4);
    return(byteCount);
}

/// <summary>
///     Encode the frame data taking into account the possibility of multiple
///     0xaa characters.
/// </summary>
int EncodeData(uint8_t *buffer, uint32_t bufferLength, uint8_t *frame)
{
    uint32_t currentByte = 0;

    if (buffer != NULL)
    {
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
    }

    return (currentByte);
}

/// <summary>
///     Generate a new application message frame.
/// </summary>
struct BufferAndLength *GenerateApplicationFrame(uint8_t frameType, uint32_t messageID, uint8_t sequenceNumber, uint8_t *buffer, uint32_t bufferLength)
{
    uint32_t frameLength = FRAME_HEADER_LENGTH + CalculateEncodedFrameLength(buffer, bufferLength);

    uint8_t *frame = pvPortMalloc(frameLength);
    memset(frame, START_OF_FRAME_CHARACTER, NUMBER_OF_START_OF_FRAME_CHARACTERS);
    uint32_t currentByte = NUMBER_OF_START_OF_FRAME_CHARACTERS;
    //
    //  Now add the frame header information.
    //
    frame[currentByte++] = frameType;
    EncodeUInt32(messageID, frame + currentByte);
    currentByte += 4;
    frame[currentByte++] = sequenceNumber;
    frame[currentByte++] = (uint8_t) (bufferLength & 0xff);
    currentByte += EncodeData(buffer, bufferLength, frame + currentByte);
    //
    //  The frame length calculated contains the header characters, CRC
    //  and the end of frame characters.  We need to take this into consideration 
    //  when the CRC is calculated as the CRC should only be calculated on
    //  the actual data plus any padding characters.
    //
    uint32_t crc = CRC32(frame + NUMBER_OF_START_OF_FRAME_CHARACTERS, frameLength - (NUMBER_OF_START_OF_FRAME_CHARACTERS + 5));
    EncodeUInt32(crc, frame + currentByte);
    currentByte += 4;
    memset(frame + currentByte, END_OF_FRAME_CHARACTER, NUMBER_OF_END_OF_FRAME_CHARACTERS);
    currentByte += NUMBER_OF_END_OF_FRAME_CHARACTERS;
    struct BufferAndLength *f = pvPortMalloc(sizeof(struct BufferAndLength));
    f->Buffer = frame;
    f->BufferLength = currentByte;
    return (f);
}

/// <summary>
///     Take a buffer of data and generate a sequences of bytes to send over the UART.
//      This sequence will contain the start of frame characters, data, CRC and end
//      of frame characters.
/// </summary>
struct BufferAndLength *GenerateTransportFrame(uint8_t *buffer, uint32_t bufferLength)
{
    uint32_t frameLength = CalculateEncodedFrameLength(buffer, bufferLength);

    uint8_t *frame = pvPortMalloc(frameLength);
    memset(frame, START_OF_FRAME_CHARACTER, NUMBER_OF_START_OF_FRAME_CHARACTERS);
    uint32_t currentByte = NUMBER_OF_START_OF_FRAME_CHARACTERS;
    currentByte += EncodeData(buffer, bufferLength, frame + currentByte);
    //
    //  The frame length calculated contains the header characters, CRC
    //  and the end of frame characters.  We need to take this into consideration 
    //  when the CRC is calculated as the CRC should only be calculated on
    //  the actual data plus any padding characters.
    //
    uint32_t crc = CRC32(frame + NUMBER_OF_START_OF_FRAME_CHARACTERS, currentByte - NUMBER_OF_START_OF_FRAME_CHARACTERS);
    EncodeUInt32(crc, frame + currentByte);
    currentByte += 4;
    memset(frame + currentByte, END_OF_FRAME_CHARACTER, NUMBER_OF_END_OF_FRAME_CHARACTERS);
    currentByte += NUMBER_OF_END_OF_FRAME_CHARACTERS;
    struct BufferAndLength *f = pvPortMalloc(sizeof(struct BufferAndLength));
    f->Buffer = frame;
    f->BufferLength = currentByte;
    return (f);
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
    struct BufferAndLength *frame = GenerateTransportFrame(buffer, TRANSPORT_MESSAGE_LENGTH);
    //
    xSemaphoreTake(xUARTTxMutex, portMAX_DELAY);
    _totalTxBytes += uart_write_bytes(STM32_UART_NUMBER, (const char *) frame->Buffer, frame->BufferLength);
    xSemaphoreGive(xUARTTxMutex);
    //
    vPortFree(frame->Buffer);
    vPortFree(frame);
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
        AcknowledgeFrame(TRANSPORT_NAK, ERR_INVALID_HEADER);
    }
    else
    {
        ResetIncomingMessage(true);
        messageHeader.interface = packet[0];
        messageHeader.function = ExtractUInt32(packet + APP_REQUEST_FUNCTION);
        messageHeader.messageID = ExtractUInt32(packet + APP_REQUEST_MESSAGE_ID);
        bytesRemaining = ExtractUInt32(packet + APP_REQUEST_PAYLOAD_LENGTH);
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
        AcknowledgeFrame(TRANSPORT_ACK, ERR_COMPLETED_OK);
    }
}

/// <summary>
///     Process an application data packet.
/// </summary>
void ProcessData(uint8_t *packet, uint8_t packetLength)
{
    if ((nextPayloadLocation != NULL) && (bytesRemaining > 0) && (packetLength <= bytesRemaining))
    {
        //
        //  TODO: Change this to use the sequence number for this message to determine where the data should go.
        //
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
        }
        AcknowledgeFrame(TRANSPORT_ACK, ERR_COMPLETED_OK);
    }
    else
    {
        AcknowledgeFrame(MSG_NAK, ERR_UNEXPECTED_DATA);
    }
}

/// <summary>
///     Process the current frame as a transport message.
/// </summary>
void ProcessTransportData(uint8_t currentFrameType)
{
    enum UARTSyncStates state = Unknown;
    switch (currentFrameType & 0x2f)
    {
        case TRANSPORT_ACK:
            state = Acknowledge;
            break;
        case TRANSPORT_NAK:
            state = NegativeAcknowledge;
            break;
        case TRANSPORT_RESET:
            ResetIncomingMessage(true);
            state = Reset;
            break;
    }
    if (xQueueSendToBack(xUARTSyncQueueHandle, &state, 0) != pdPASS)
    {
        ESP_LOGI(__func__, "Sending synchronisation message.");
    }
}

/// <summary>
///     A valid frame has been received, work out what action should be taken.
/// </summary>
/// <remarks>
///     Note that it is responsibility of the processor to work out if an ACK / NAK 
///     should be sent.
/// </remarks>
void ProcessApplicationDataFrame(uint8_t frameType, uint8_t *packet, uint8_t packetLength)
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

/// <summary>
///     Send a frame to the STM32 and let the caller know the result.
/// </summary>
/// <param name="frame">Data frame to send.</param>
/// <param name="frameType>Name of the frame type being sent, this is used for debugging purposes.</param>
enum UARTSyncStates SendFrame(struct BufferAndLength *frame, const char * frameType)
{
    enum UARTSyncStates result = Error;
    int retryCount = 0;

    //
    //  This queue needs to be empty before we send a frame otherwise we could
    //  receive and ACK / NAK for the wrong message.
    //
    xQueueReset(xUARTSyncQueueHandle);
    while ((result != Acknowledge) && (retryCount < _maximumRetryCount))
    {
        xSemaphoreTake(xUARTTxMutex, portMAX_DELAY);
        _totalTxBytes += uart_write_bytes(STM32_UART_NUMBER, (const char *) frame->Buffer, frame->BufferLength);
        _totalFramesSent++;
        xSemaphoreGive(xUARTTxMutex);
        retryCount++;
        BaseType_t queueResult = xQueueReceive(xUARTSyncQueueHandle, &result, _uartMaximumSendResponsePeriod);
        if (queueResult != pdTRUE)
        {
            result = Error;
        }
    }
    if (result != Acknowledge)
    {
        if (retryCount < _maximumRetryCount)
        {
            result = Error;
        }
        else
        {
            result = Timeout;
        }
    }

    return (result);
}

/// <summary>
///     UART transmit task.
/// </summary>
void UARTTxTask(void *pvParameters)
{
    while (true)
    {
        struct ApplicationResponse message;
        enum UARTSyncStates sendResult;

        BaseType_t result = xQueueReceive(UARTTransmitQueueHandle, &message, portMAX_DELAY);        
        if (result == pdTRUE)
        {
            //
            //  Put together the header for the message.
            //
            uint8_t buffer[APP_RESPONSE_HEADER_LENGTH];

            memset(buffer, 0, APP_RESPONSE_HEADER_LENGTH);
            buffer[APP_RESPONSE_INTERFACE] = message.interface;
            EncodeUInt32(message.code, buffer + APP_RESPONSE_CODE);
            EncodeUInt32(message.messageID, buffer + APP_RESPONSE_MESSAGE_ID);
            EncodeUInt32(message.payloadLength, buffer + APP_RESPONSE_PAYLOAD_LENGTH);
            //
            //  Generate an encoded header frame and send it.
            //
            struct BufferAndLength *frame = GenerateApplicationFrame(PKT_DATA | DATA_HEADER, message.messageID, 0, buffer, APP_RESPONSE_HEADER_LENGTH);
            sendResult = SendFrame(frame, "header");
            if (frame->Buffer != NULL)
            {
                vPortFree(frame->Buffer);
            }
            vPortFree(frame);
            frame = NULL;
            //
            //  TODO: Work out what to do about errors.
            //


            //
            //  Now break the payload down into frames and send the frames to the STM32.
            //
            int amountLeftToSend = message.payloadLength;
            int start = 0;
            uint8_t sequenceNumber = 1;
            uint8_t currentFrameBufferLength = UART_MAXIMUM_FRAME_LENGTH;
            uint8_t *frameBuffer = pvPortMalloc(currentFrameBufferLength);
            while (amountLeftToSend > 0)
            {
                uint8_t length = (uint8_t) ((amountLeftToSend < UART_MAXIMUM_FRAME_LENGTH ? amountLeftToSend : UART_MAXIMUM_FRAME_LENGTH) & 0xff);
                if (length != currentFrameBufferLength)
                {
                    vPortFree(frameBuffer);
                    currentFrameBufferLength = length;
                    frameBuffer = pvPortMalloc(currentFrameBufferLength);
                }
                memcpy(frameBuffer, message.payload + start, length);
                start += length;
                amountLeftToSend -= length;
                //
                //  Generate an encoded header frame and send it.
                //
                frame = GenerateApplicationFrame(PKT_DATA | DATA_PAYLOAD, message.messageID, sequenceNumber++, frameBuffer, length);
                sendResult = SendFrame(frame, "payload");
                if (frame->Buffer != NULL)
                {
                    vPortFree(frame->Buffer);
                }
                vPortFree(frame);
                //
                //  TODO: Work out what to do about errors.
                //
            }
            //
            //  Message has been sent so it is time to clean up the heap.
            //
            if (frameBuffer != NULL)
            {
                vPortFree(frameBuffer);
            }
            if (message.payloadLength > 0)
            {
                vPortFree(message.payload);
            }
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
///     Decode data received from the serial port.
/// </summary>
/// <remarks>
///     This process has been abstracted from the serial received event
///     to aid debugging.
///
///     The <i>amount</i> parameter is necessary as the buffer is normally
///     larger than the amount of data that has been received.
/// </remarks>
/// <param name="buffer">Data to be decoded.</param>
/// <param name="amount">Amount of data to decode.</param>
void DecodeSerialData(uint8_t *buffer, int amount)
{
    _totalRxBytes += amount;
    for (int index = 0; index < amount; index++)
    {
        uint8_t byte = buffer[index];

        if (_startOfFrameByteCount == 2)
        {
            _startOfFrameByteCount = 0;
            switch (byte)
            {
                case START_OF_FRAME_CHARACTER:
                    _communicationState = FrameType;
                    break;
                case PADDING_CHARACTER:
                    break;
                default:
                    _communicationState = StartOfFrame;
                    break;
            }
        }
        else
        {
            if (byte == START_OF_FRAME_CHARACTER)
            {
                _startOfFrameByteCount++;
            }
            else
            {
                _startOfFrameByteCount = 0;
            }
            switch (_communicationState)
            {
                case StartOfFrame:
                    //
                    //  We don't need to do anything here as Start of Frame
                    //  is dealt with above.
                    //
                    break;
                case FrameType:
                    _currentFrameType = byte;
                    if ((_currentFrameType & PKT_TRANSPORT) > 0)
                    {
                        _communicationState = StatusCode;
                        _statusCode = 0;
                    }
                    else
                    {
                        _communicationState = MessageID;
                        _messageID = 0;
                    }
                    _calculatedChecksum = CRC32_SEED;
                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, byte);
                    _byteNumber = 0;
                    break;
                case StatusCode:
                    _statusCode |= (byte << (_byteNumber * 8));
                    _byteNumber++;
                    if (_byteNumber == 4)
                    {
                        _communicationState = CRC;
                        _byteNumber = 0;
                        _originalChecksum = 0;
                    }
                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, byte);
                    break;
                case MessageID:
                    _messageID |= (byte << (_byteNumber * 8));
                    _byteNumber++;
                    if (_byteNumber == 4)
                    {
                        _communicationState = SequenceNumber;                                    
                    }
                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, byte);
                    break;
                case SequenceNumber:
                    _frameSequenceNumber = byte;
                    _communicationState = Length;
                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, byte);
                    break;
                case Length:
                    _framePayloadLength = byte;
                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, byte);
                    if (_framePayloadLength > 0)
                    {
                        _framePayload = pvPortMalloc(_framePayloadLength);
                        _framePayloadIndex = 0;
                        _communicationState = Payload;
                    }
                    else
                    {
                        _communicationState = CRC;
                        _originalChecksum = 0;
                        _byteNumber = 0;
                    }
                    break;
                case Payload:
                    _framePayload[_framePayloadIndex] = byte;
                    _framePayloadIndex++;
                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, byte);
                    if (_framePayloadIndex == _framePayloadLength)
                    {
                        _communicationState = CRC;
                        _originalChecksum = 0;
                        _byteNumber = 0;
                    }
                    break;
                case CRC:
                    _originalChecksum |= (byte << (_byteNumber * 8));
                    _byteNumber++;
                    if (_byteNumber == 4)
                    {
                        _communicationState = EndOfFrame;                                    
                    }
                    break;
                case EndOfFrame:
                    if ((byte == END_OF_FRAME_CHARACTER) && (_calculatedChecksum == _originalChecksum)) 
                    {
                        if (_currentFrameType & PKT_TRANSPORT)
                        {
                            ProcessTransportData(_currentFrameType);
                        }
                        else
                        {
                            ProcessApplicationDataFrame(_currentFrameType, _framePayload, _framePayloadLength);
                        }
                    }
                    else
                    {
                        if (byte != END_OF_FRAME_CHARACTER)
                        {
                            AcknowledgeFrame(TRANSPORT_NAK, ERR_MISSING_END_OF_FRAME_MARKER);
                        }
                        else
                        {
                            AcknowledgeFrame(TRANSPORT_NAK, ERR_CRC_ERROR);
                        }
                    }
                    if (_framePayload != NULL)
                    {
                        vPortFree(_framePayload);
                        _framePayload = NULL;
                    }
                    _communicationState = StartOfFrame;
                    break;
                default:
                    _communicationState = StartOfFrame;
                    break;
            }
        }
    }
}

/// <summary>
///     UART data received interrupt task.
/// </summary>
void UARTRxTask(void *pvParameters)
{
    uart_event_t event;
    uint8_t* data = (uint8_t *) pvPortMalloc(RX_BUF_SIZE);

    while (true)
    {
        int dataLeftToRead = 0;
        int amountToRead = 0;
        if (xQueueReceive(UARTRxQueueHandle, (void *) &event, portMAX_DELAY))
        {
            memset(data, 0, RX_BUF_SIZE);
            switch (event.type)
            {
                case UART_DATA:
                    dataLeftToRead = event.size;
                    while (dataLeftToRead > 0)
                    {
                        amountToRead = dataLeftToRead <= RX_BUF_SIZE ? dataLeftToRead : RX_BUF_SIZE;
                        memset(data, 0, RX_BUF_SIZE);
                        uart_read_bytes(STM32_UART_NUMBER, data, amountToRead, portMAX_DELAY);
                        DecodeSerialData(data, amountToRead);
                        dataLeftToRead -= amountToRead;
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
    if (_framePayload != NULL)
    {
        vPortFree(_framePayload);
        _framePayload = NULL;
    }
    vTaskDelete(xUARTReceiveTaskHandle);
}
