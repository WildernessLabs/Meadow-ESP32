/*
 *  UART.cpp
 *
 *  Code (methods etc.) to implement the UART communications class.
 *
 *  This class provides an error tolerant way for the ESP32 and the STM32
 *  to communicate.
 *
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

#include "UART.hpp"
#include "WiFi.hpp"
#include "Tasks.hpp"
#include "Messaging.h"
#include "ErrorCodes.h"
#include "Encoders.hpp"

/*
 *      External declaration for the WiFi instance.
 */
extern WiFi *wifi;

/*
 * ---------------------------------------------------------------------------
 *
 *                         Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */


/*
 *      Default constructor for the UART class.
 */
UART::UART()
{
    messageHeader.interface = 0;
    messageHeader.function = 0;
    messageHeader.messageID = 0;
    messageHeader.payload = NULL;
    messageHeader.payloadLength = 0;
    bytesRemaining  = 0;
    nextPayloadLocation = NULL;
}

/*
 *      Destructor for the UART class.
 */
UART::~UART()
{
}

/*
 * ---------------------------------------------------------------------------
 *
 *                           Getters and Setters
 *
 * ---------------------------------------------------------------------------
 */

QueueHandle_t UART::GetUartRxQueueHandle() const
{
    return xUARTReceiveQueueHandle;
}

void UART::SetUartRxQueueHandle(QueueHandle_t uartRxQueueHandle = NULL)
{
    xUARTReceiveQueueHandle = uartRxQueueHandle;
}

QueueHandle_t UART::GetUartTransmitQueueHandle() const
{
    return xUARTTransmitQueueHandle;
}

void UART::SetUartTransmitQueueHandle(QueueHandle_t uartTransmitQueueHandle = NULL)
{
    xUARTTransmitQueueHandle = uartTransmitQueueHandle;
}

TaskHandle_t UART::GetUartReceiveTaskHandle() const
{
    return xUARTReceiveTaskHandle;
}

void UART::SetUartReceiveTaskHandle(TaskHandle_t uartReceiveTaskHandle = NULL)
{
    xUARTReceiveTaskHandle = uartReceiveTaskHandle;
}

TaskHandle_t UART::GetUartTransmitTaskHandle() const
{
    return xUARTTransmitTaskHandle;
}

void UART::SetUartTransmitTaskHandle(TaskHandle_t uartTransmitTaskHandle = NULL)
{
    xUARTTransmitTaskHandle = uartTransmitTaskHandle;
}

/*
 * ---------------------------------------------------------------------------
 *
 *                                  Methods
 *
 * ---------------------------------------------------------------------------
 */

/*
 *      Configure the UART and install the driver.
 *
 *      UART should be configured as 115200 baud, no parity, one stop bit, 8 data bits.
 */
void UART::Setup()
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
    _uartMaximumSendResponsePeriod = 500 / portTICK_PERIOD_MS;
    //
    uart_param_config(STM32UartNumber, &uart_config);
    uart_set_pin(STM32UartNumber, TxPin, RxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    QueueHandle_t rxQueue;
    uart_driver_install(STM32UartNumber, RxBufferSize * 2, TxBufferSize * 2, QueueLength, &rxQueue, 0);
    uart->SetUartRxQueueHandle(rxQueue);
    xUARTTxMutex = xSemaphoreCreateMutex();
    xUARTSyncQueueHandle = xQueueCreate(1, sizeof(enum UARTSyncStates));
    memset(_startOfFrameCharacters, StartOfFrameCharacter, NumberOfStartOfFrameCharacters);
    memset(_endOfFrameCharacters, EndOfFrameCharacter, NumberOfEndOfFrameCharacters);

    xUARTTransmitQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, sizeof(ApplicationResponse));
    if (xUARTTransmitQueueHandle == NULL)
    {
        ESP_LOGI(__func__, "Cannot allocate UART transmit queue.");
    }
    xTaskCreate(UARTRxTask, UART_RX_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 5, &xUARTReceiveTaskHandle);
    xTaskCreate(UARTTxTask, UART_TX_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 5, &xUARTTransmitTaskHandle);
}

/*
 *      Clean up following a failure in the Rx task.
 */
void UART::RxCleanup()
{
    if (_framePayload != NULL)
    {
        vPortFree(_framePayload);
        _framePayload = NULL;
    }
}

/*
 *      Reset the message header and delete the message body storage.
 */
void UART::ResetIncomingMessage(bool releasePayloadMemory)
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

/*
 *      Add the message to the appropriate queue if possible.
 */
uint32_t UART::QueueMessage()
{
    QueueHandle_t queueHandle = NULL;

    switch (messageHeader.interface)
    {
        case INTERFACE_WIFI:
            queueHandle = wifi->GetQueueHandle();
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

/*
 *      Add a response to the UART Tx queue.
 *
 *      @param response
 *          Response to be queued for sending to the STM32.
 */
void UART::QueueResponse(ApplicationResponse *response)
{
    if (xQueueSendToBack(xUARTTransmitQueueHandle, response, 0) != pdPASS)
    {
        ESP_LOGI(__func__, "Error sending response, cannot queue message");
    }
}

/*
 *      Work out how big the frame buffer should be for the message that
 *      is going to be sent.  This will take into account the padding
 *      and CRC etc.
 *
 *      @param buffer
 *          Buffer of data to calculate the encoded length.
 *
 *      @param bufferLength
 *          Amount of data in the buffer.
 */
uint32_t UART::CalculateEncodedFrameLength(uint8_t *buffer, uint8_t bufferLength)
{
    int startOfFrameCharacterCount = 0;
    int byteCount = 0;

    for (int index = 0; index < bufferLength; index++)
    {
        if (buffer[index] == StartOfFrameCharacter)
        {
            startOfFrameCharacterCount++;
            if (startOfFrameCharacterCount == (NumberOfStartOfFrameCharacters - 1))
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
    byteCount += (NumberOfStartOfFrameCharacters + NumberOfEndOfFrameCharacters + 4);
    return(byteCount);
}

/*
 *      Encode the frame data taking into account the possibility of multiple
 *      0xaa characters.
 *
 *      @param buffer
 *          Buffer of data to calculate the encoded length.
 *
 *      @param bufferLength
 *          Amount of data in the buffer.
 *
 *       @param frame
 *          Pointer to the memory that will hold the encoded data.
 */
int UART::EncodeData(uint8_t *buffer, uint32_t bufferLength, uint8_t *frame)
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
            if (buffer[index] == StartOfFrameCharacter)
            {
                startOfFrameCharacterCount++;
                if (startOfFrameCharacterCount == (NumberOfStartOfFrameCharacters - 1))
                {
                    frame[currentByte++] = PaddingCharacter;
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

/*
 *      Generate a new application frame.
 *
 *      @param frameType
 *          Frame type (header of data).
 *
 *      @param messageID
 *          ID of the message being sent.
 *
 *      @param sequenceNumber
 *          Sequence number of this frame in the message.  Each frame will be sequentially
 *          numbered from 0 for the header and 1..n for the data frames.
 *
 *      @param buffer
 *          Buffer of data to calculate the encoded length.
 *
 *      @param bufferLength
 *          Amount of data in the buffer.
 */
struct BufferAndLength *UART::GenerateApplicationFrame(uint8_t frameType, uint32_t messageID, uint8_t sequenceNumber, uint8_t *buffer, uint32_t bufferLength)
{
    uint32_t frameLength = FRAME_HEADER_LENGTH + CalculateEncodedFrameLength(buffer, bufferLength);

    uint8_t *frame = (uint8_t *) pvPortMalloc(frameLength);
    memset(frame, StartOfFrameCharacter, NumberOfStartOfFrameCharacters);
    uint32_t currentByte = NumberOfStartOfFrameCharacters;
    //
    //  Now add the frame header information.
    //
    frame[currentByte++] = frameType;
    Encoders::EncodeUInt32(messageID, frame + currentByte);
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
    uint32_t crc = Encoders::CRC32(frame + NumberOfStartOfFrameCharacters, frameLength - (NumberOfStartOfFrameCharacters + 5));
    Encoders::EncodeUInt32(crc, frame + currentByte);
    currentByte += 4;
    memset(frame + currentByte, EndOfFrameCharacter, NumberOfEndOfFrameCharacters);
    currentByte += NumberOfEndOfFrameCharacters;
    BufferAndLength *f = (BufferAndLength *) pvPortMalloc(sizeof(BufferAndLength));
    f->Buffer = frame;
    f->BufferLength = currentByte;
    //
    //  The following lines are for debugging and will dump the frame being sent to the LOG UART.
    //
    //  ESP_LOGI(__func__, "Sending frame:");
    //  ESP_LOG_BUFFER_HEXDUMP(__func__, frame, currentByte, ESP_LOG_INFO);
    //
    return (f);
}

/*
 *      Take a buffer of data and generate a sequences of bytes to send over the UART.
 *
 *      This sequence will contain the start of frame characters, data, CRC and end of frame characters.
 *
 *      @param buffer
 *          Buffer of data to calculate the encoded length.
 *
 *      @param bufferLength
 *          Amount of data in the buffer.
 */
struct BufferAndLength *UART::GenerateTransportFrame(uint8_t *buffer, uint32_t bufferLength)
{
    uint32_t frameLength = CalculateEncodedFrameLength(buffer, bufferLength);

    uint8_t *frame = (uint8_t *) pvPortMalloc(frameLength);
    memset(frame, StartOfFrameCharacter, NumberOfStartOfFrameCharacters);
    uint32_t currentByte = NumberOfStartOfFrameCharacters;
    currentByte += EncodeData(buffer, bufferLength, frame + currentByte);
    //
    //  The frame length calculated contains the header characters, CRC
    //  and the end of frame characters.  We need to take this into consideration
    //  when the CRC is calculated as the CRC should only be calculated on
    //  the actual data plus any padding characters.
    //
    uint32_t crc = Encoders::CRC32(frame + NumberOfStartOfFrameCharacters, currentByte - NumberOfStartOfFrameCharacters);
    Encoders::EncodeUInt32(crc, frame + currentByte);
    currentByte += 4;
    memset(frame + currentByte, EndOfFrameCharacter, NumberOfStartOfFrameCharacters);
    currentByte += NumberOfStartOfFrameCharacters;
    BufferAndLength *f = (BufferAndLength *) pvPortMalloc(sizeof(struct BufferAndLength));
    f->Buffer = frame;
    f->BufferLength = currentByte;
    return (f);
}

/*
 *      Send a positive (ACK) or negative (NAK) acknowledgement along with a response code.
 *
 *      @param ackNak
 *          Code for and ACK or NAK (as an integer)
 *
 *      @param statuc
 *          Status code to be encoded in the transport frame.
 */
void UART::AcknowledgeFrame(uint8_t ackNak, uint32_t status)
{
    uint8_t buffer[TRANSPORT_MESSAGE_LENGTH];
    memset(buffer, 0, TRANSPORT_MESSAGE_LENGTH);
    buffer[TRANSPORT_FRAME_TYPE] = PKT_TRANSPORT | ackNak;
    Encoders::EncodeUInt32(status, buffer + TRANSPORT_STATUS_CODE);
    struct BufferAndLength *frame = GenerateTransportFrame(buffer, TRANSPORT_MESSAGE_LENGTH);
    //
    xSemaphoreTake(xUARTTxMutex, portMAX_DELAY);
    _totalTxBytes += uart_write_bytes(STM32UartNumber, (const char *) frame->Buffer, frame->BufferLength);
    xSemaphoreGive(xUARTTxMutex);
    //
    vPortFree(frame->Buffer);
    vPortFree(frame);
}

/*
 *      Process an incoming application header packet.
 *
 *      This method will extract the message information (ID, interface etc) from the incoming
 *      message packet.
 *
 *      @param packet
 *          Pointer to the header packet to be processed.
 *
 *      @param packetLength
 *          Amount of data in the packet.
 */
void UART::ProcessHeader(uint8_t *packet, uint8_t packetLength)
{
    if (packetLength != APP_REQUEST_HEADER_LENGTH)
    {
        AcknowledgeFrame(TRANSPORT_NAK, ERR_INVALID_HEADER);
    }
    else
    {
        ResetIncomingMessage(true);
        messageHeader.interface = packet[0];
        messageHeader.function = Encoders::ExtractUInt32(packet + APP_REQUEST_FUNCTION);
        messageHeader.messageID = Encoders::ExtractUInt32(packet + APP_REQUEST_MESSAGE_ID);
        bytesRemaining = Encoders::ExtractUInt32(packet + APP_REQUEST_PAYLOAD_LENGTH);
        messageHeader.payloadLength = bytesRemaining;
        if (messageHeader.payloadLength > 0)
        {
            messageHeader.payload = (uint8_t *) pvPortMalloc(messageHeader.payloadLength);
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

/*
 *      Process an application data packet.
 *
 *      The application frames will be reassembled into a larger message (if required).  The message
 *      will be added to the appropriate message queue for processing once a full message has been
 *      received.
 *
 *      @param packet
 *          Pointer to the header packet to be processed.
 *
 *      @param packetLength
 *          Amount of data in the packet.
 */
void UART::ProcessData(uint8_t *packet, uint8_t packetLength)
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

/*
 *      Process the current frame as a transport message.
 *
 *      @param currentFrameType
 *          Transport frame type.
 */
void UART::ProcessTransportData(uint8_t currentFrameType)
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

/*
 *      A valid frame has been received, work out what action should be taken.
 *
 *      Note that it is responsibility of the processor to work out if an ACK / NAK should be sent.
 *
 *      @param frameType
 *          Frame type information from the frame.
 *
 *      @param packet
 *          Frame to be processed.
 *
 *      @param packetLength
 *          Amount of data in the frame.
 */
void UART::ProcessApplicationDataFrame(uint8_t frameType, uint8_t *packet, uint8_t packetLength)
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

/*
 *      Send a frame to the STM32 and let the caller know the result.
 *
 *      @param frame
 *          Data frame to send.
 *
 *      @param frameType
 *          Name of the frame type being sent, this is used for debugging purposes.
 */
UART::UARTSyncStates UART::SendFrame(struct BufferAndLength *frame, const char * frameType)
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
        _totalTxBytes += uart_write_bytes(STM32UartNumber, (const char *) frame->Buffer, frame->BufferLength);
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

/*
 *      Decode data received from the serial port.
 *
 *      This process has been abstracted from the serial received event to aid debugging.
 *
 *      The amount parameter is necessary as the buffer is normally larger than the amount of data
 *      that has been received.
 *
 *      @param buffer
 *          Data to be decoded.
 *
 *      @param amount
 *          Amount of data to decode.
 */
void UART::DecodeSerialData(uint8_t *buffer, int amount)
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
                case StartOfFrameCharacter:
                    _communicationState = FrameType;
                    break;
                case PaddingCharacter:
                    break;
                default:
                    _communicationState = StartOfFrame;
                    break;
            }
        }
        else
        {
            if (byte == StartOfFrameCharacter)
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
                    _calculatedChecksum = Encoders::CRC32Seed;
                    _calculatedChecksum = Encoders::ProgressiveCRC32(_calculatedChecksum, byte);
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
                    _calculatedChecksum = Encoders::ProgressiveCRC32(_calculatedChecksum, byte);
                    break;
                case MessageID:
                    _messageID |= (byte << (_byteNumber * 8));
                    _byteNumber++;
                    if (_byteNumber == 4)
                    {
                        _communicationState = SequenceNumber;
                    }
                    _calculatedChecksum = Encoders::ProgressiveCRC32(_calculatedChecksum, byte);
                    break;
                case SequenceNumber:
                    _frameSequenceNumber = byte;
                    _communicationState = Length;
                    _calculatedChecksum = Encoders::ProgressiveCRC32(_calculatedChecksum, byte);
                    break;
                case Length:
                    _framePayloadLength = byte;
                    _calculatedChecksum = Encoders::ProgressiveCRC32(_calculatedChecksum, byte);
                    if (_framePayloadLength > 0)
                    {
                        _framePayload = (uint8_t *) pvPortMalloc(_framePayloadLength);
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
                    _calculatedChecksum = Encoders::ProgressiveCRC32(_calculatedChecksum, byte);
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
                    if ((byte == EndOfFrameCharacter) && (_calculatedChecksum == _originalChecksum))
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
                        if (byte != EndOfFrameCharacter)
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

/*
 *      UART data received interrupt task.
 *
 *      @param message
 *          Application response.
 */
void UART::DispatchRxMessage(ApplicationResponse *message)
{
    uart_event_t event;
    uint8_t* data = (uint8_t *) pvPortMalloc(RxBufferSize);

    while (true)
    {
        int dataLeftToRead = 0;
        int amountToRead = 0;
        if (xQueueReceive(xUARTReceiveQueueHandle, (void *) &event, portMAX_DELAY))
        {
            memset(data, 0, RxBufferSize);
            switch (event.type)
            {
                case UART_DATA:
                    dataLeftToRead = event.size;
                    while (dataLeftToRead > 0)
                    {
                        amountToRead = dataLeftToRead <= RxBufferSize ? dataLeftToRead : RxBufferSize;
                        memset(data, 0, RxBufferSize);
                        uart_read_bytes(STM32UartNumber, data, amountToRead, portMAX_DELAY);
                        DecodeSerialData(data, amountToRead);
                        dataLeftToRead -= amountToRead;
                    }
                    break;
                case UART_BUFFER_FULL:
                case UART_FIFO_OVF:
                    //
                    //  Flush the input buffer in the case of a full ring buffer or a FIFO overflow.
                    //
                    uart_flush_input(STM32UartNumber);
                    xQueueReset(xUARTReceiveQueueHandle);
                    break;
                default:
                    uart_flush_input(STM32UartNumber);
                    xQueueReset(xUARTReceiveQueueHandle);
                    xQueueReset(xUARTTransmitQueueHandle);
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

/*
 *      Break the message into header and data frames and send the message to the STM32.
 *
 *      @param message
 *          Message to be sent to the STM32.
 */
void UART::Send(ApplicationResponse *message)
{
    enum UARTSyncStates sendResult;

    //
    //  Put together the header for the message.
    //
    uint8_t buffer[APP_RESPONSE_HEADER_LENGTH];

    memset(buffer, 0, APP_RESPONSE_HEADER_LENGTH);
    buffer[APP_RESPONSE_INTERFACE] = message->interface;
    Encoders::EncodeUInt32(message->code, buffer + APP_RESPONSE_CODE);
    Encoders::EncodeUInt32(message->messageID, buffer + APP_RESPONSE_MESSAGE_ID);
    Encoders::EncodeUInt32(message->payloadLength, buffer + APP_RESPONSE_PAYLOAD_LENGTH);
    //
    //  Generate an encoded header frame and send it.
    //
    BufferAndLength *frame = GenerateApplicationFrame(PKT_DATA | DATA_HEADER, message->messageID, 0, buffer, APP_RESPONSE_HEADER_LENGTH);
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
    int amountLeftToSend = message->payloadLength;
    int start = 0;
    uint8_t sequenceNumber = 1;
    uint8_t currentFrameBufferLength = MaximumFrameLength;
    uint8_t *frameBuffer = (uint8_t *) pvPortMalloc(currentFrameBufferLength);
    while (amountLeftToSend > 0)
    {
        uint8_t length = (uint8_t) ((amountLeftToSend < MaximumFrameLength ? amountLeftToSend : MaximumFrameLength) & 0xff);
        if (length != currentFrameBufferLength)
        {
            vPortFree(frameBuffer);
            currentFrameBufferLength = length;
            frameBuffer = (uint8_t *) pvPortMalloc(currentFrameBufferLength);
        }
        memcpy(frameBuffer, message->payload + start, length);
        start += length;
        amountLeftToSend -= length;
        //
        //  Generate an encoded header frame and send it.
        //
        frame = GenerateApplicationFrame(PKT_DATA | DATA_PAYLOAD, message->messageID, sequenceNumber++, frameBuffer, length);
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
    if (message->payloadLength > 0)
    {
        vPortFree(message->payload);
    }
}

