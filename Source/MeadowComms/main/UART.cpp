/*
 * UART.cpp
 *
 *  Created on: 18 Jan 2019
 *      Author: markstevens
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

extern WiFi *wifi;

// ---------------------------------------------------------------------------
//
//                         Constructors and destructor
//
// ---------------------------------------------------------------------------


UART::UART()
{
}

UART::~UART()
{
}

// ---------------------------------------------------------------------------
//
//                           Getters and Setters
//
// ---------------------------------------------------------------------------

QueueHandle_t UART::GetUartRxQueueHandle() const
{
	return UARTRxQueueHandle;
}

void UART::SetUartRxQueueHandle(QueueHandle_t uartRxQueueHandle = NULL)
{
	UARTRxQueueHandle = uartRxQueueHandle;
}

QueueHandle_t UART::GetUartTransmitQueueHandle() const
{
	return UARTTransmitQueueHandle;
}

void UART::SetUartTransmitQueueHandle(QueueHandle_t uartTransmitQueueHandle = NULL)
{
	UARTTransmitQueueHandle = uartTransmitQueueHandle;
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
    _uartMaximumSendResponsePeriod = 250 / portTICK_PERIOD_MS;
    //
    uart_param_config(STM32_UART_NUMBER, &uart_config);
    uart_set_pin(STM32_UART_NUMBER, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(STM32_UART_NUMBER, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, UART_QUEUE_LENGTH, &UARTRxQueueHandle, 0);
    xUARTTxMutex = xSemaphoreCreateMutex();
    xUARTSyncQueueHandle = xQueueCreate(1, sizeof(enum UARTSyncStates));
    memset(_startOfFrameCharacters, START_OF_FRAME_CHARACTER, NUMBER_OF_START_OF_FRAME_CHARACTERS);
    memset(_endOfFrameCharacters, END_OF_FRAME_CHARACTER, NUMBER_OF_END_OF_FRAME_CHARACTERS);
    xTaskCreate(UARTRxTask, UART_RX_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 5, &xUARTReceiveTaskHandle);
    xTaskCreate(UARTTxTask, UART_TX_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 5, &xUARTTransmitTaskHandle);
}

void UART::RxCleanup()
{
    if (_framePayload != NULL)
    {
        vPortFree(_framePayload);
        _framePayload = NULL;
    }
}

/// <summary>
///     Reset the message header and delete the message body storage.
/// <summary>
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

/// <summary>
///     Add the message to the appropriate queue if possible.
/// </summary>
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

/// <summary>
///     Add a response to the UART Tx queue.
/// </summary>
/// <param name="response">Response to be queued for sending to the STM32.</param>
void UART::QueueUARTResponse(struct ApplicationResponse *response)
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
uint32_t UART::CalculateEncodedFrameLength(uint8_t *buffer, uint8_t bufferLength)
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
struct BufferAndLength *UART::GenerateApplicationFrame(uint8_t frameType, uint32_t messageID, uint8_t sequenceNumber, uint8_t *buffer, uint32_t bufferLength)
{
    uint32_t frameLength = FRAME_HEADER_LENGTH + CalculateEncodedFrameLength(buffer, bufferLength);

    uint8_t *frame = (uint8_t *) pvPortMalloc(frameLength);
    memset(frame, START_OF_FRAME_CHARACTER, NUMBER_OF_START_OF_FRAME_CHARACTERS);
    uint32_t currentByte = NUMBER_OF_START_OF_FRAME_CHARACTERS;
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
    uint32_t crc = CRC32(frame + NUMBER_OF_START_OF_FRAME_CHARACTERS, frameLength - (NUMBER_OF_START_OF_FRAME_CHARACTERS + 5));
    Encoders::EncodeUInt32(crc, frame + currentByte);
    currentByte += 4;
    memset(frame + currentByte, END_OF_FRAME_CHARACTER, NUMBER_OF_END_OF_FRAME_CHARACTERS);
    currentByte += NUMBER_OF_END_OF_FRAME_CHARACTERS;
    BufferAndLength *f = (BufferAndLength *) pvPortMalloc(sizeof(struct BufferAndLength));
    f->Buffer = frame;
    f->BufferLength = currentByte;
    return (f);
}

/// <summary>
///     Take a buffer of data and generate a sequences of bytes to send over the UART.
//      This sequence will contain the start of frame characters, data, CRC and end
//      of frame characters.
/// </summary>
struct BufferAndLength *UART::GenerateTransportFrame(uint8_t *buffer, uint32_t bufferLength)
{
    uint32_t frameLength = CalculateEncodedFrameLength(buffer, bufferLength);

    uint8_t *frame = (uint8_t *) pvPortMalloc(frameLength);
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
    Encoders::EncodeUInt32(crc, frame + currentByte);
    currentByte += 4;
    memset(frame + currentByte, END_OF_FRAME_CHARACTER, NUMBER_OF_END_OF_FRAME_CHARACTERS);
    currentByte += NUMBER_OF_END_OF_FRAME_CHARACTERS;
    BufferAndLength *f = (BufferAndLength *) pvPortMalloc(sizeof(struct BufferAndLength));
    f->Buffer = frame;
    f->BufferLength = currentByte;
    return (f);
}

/// <summary>
///     Send a positive (ACK) or negative (NAK) acknowledgement along with a response code.
/// </summary>
void UART::AcknowledgeFrame(uint8_t ackNak, uint32_t status)
{
    uint8_t buffer[TRANSPORT_MESSAGE_LENGTH];
    memset(buffer, 0, TRANSPORT_MESSAGE_LENGTH);
    buffer[TRANSPORT_FRAME_TYPE] = PKT_TRANSPORT | ackNak;
    Encoders::EncodeUInt32(status, buffer + TRANSPORT_STATUS_CODE);
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

/// <summary>
///     Process an application data packet.
/// </summary>
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

/// <summary>
///     Process the current frame as a transport message.
/// </summary>
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

/// <summary>
///     A valid frame has been received, work out what action should be taken.
/// </summary>
/// <remarks>
///     Note that it is responsibility of the processor to work out if an ACK / NAK
///     should be sent.
/// </remarks>
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

/// <summary>
///     Send a frame to the STM32 and let the caller know the result.
/// </summary>
/// <param name="frame">Data frame to send.</param>
/// <param name="frameType>Name of the frame type being sent, this is used for debugging purposes.</param>
enum UARTSyncStates UART::SendFrame(struct BufferAndLength *frame, const char * frameType)
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
void UART::DispatchRxMessage(struct ApplicationResponse *message)
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

/// <summary>
///     UART transmit task.
/// </summary>
void UART::Send(struct ApplicationResponse *message)
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
	struct BufferAndLength *frame = GenerateApplicationFrame(PKT_DATA | DATA_HEADER, message->messageID, 0, buffer, APP_RESPONSE_HEADER_LENGTH);
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
	uint8_t currentFrameBufferLength = UART_MAXIMUM_FRAME_LENGTH;
	uint8_t *frameBuffer = (uint8_t *) pvPortMalloc(currentFrameBufferLength);
	while (amountLeftToSend > 0)
	{
		uint8_t length = (uint8_t) ((amountLeftToSend < UART_MAXIMUM_FRAME_LENGTH ? amountLeftToSend : UART_MAXIMUM_FRAME_LENGTH) & 0xff);
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

