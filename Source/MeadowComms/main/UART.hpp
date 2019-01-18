/*
 * UART.hpp
 *
 *  Created on: 18 Jan 2019
 *      Author: markstevens
 */
#ifndef MAIN_UART_HPP_
#define MAIN_UART_HPP_


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

#include "Messaging.h"
#include "ErrorCodes.h"
#include "Encoders.hpp"

//
//  Number of bytes in the Rx and Tx buffers.
//
static const int RX_BUF_SIZE = 1024;
static const int TX_BUF_SIZE = 1024;

//
//  Which pins are used UART.
//
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

//
//  UART used for communication with the STM32.
//
#define STM32_UART_NUMBER (UART_NUM_1)

//
//  A repeating pattern of the character indicates a start of message.
//
#define START_OF_FRAME_CHARACTER    (0xaa)

//
//  Character marking the end of a data frame.
//
#define END_OF_FRAME_CHARACTER      (0x55)

//
//  The filler character is used to send a sequence of Start of Frame
//  characters in the binary data.
//
#define PADDING_CHARACTER            (0x52)

//
//  Number of characters to be used in the pattern matching.
//
#define NUMBER_OF_START_OF_FRAME_CHARACTERS     (3)

//
//  Number of end of frame characters.
//
#define NUMBER_OF_END_OF_FRAME_CHARACTERS       (1)

//
//  Length of the UART queue.
//
#define UART_QUEUE_LENGTH               (10)

//
//  Maximum number of unencoded bytes in a frame.
//
#define UART_MAXIMUM_FRAME_LENGTH       (255)

//
//  Default maximum number of time the UART will retry sending a message.
//
#define UART_DEFAULT_MAXIMUM_RETRIES    (3)

//
//  Communication states for the UART.
//
enum CommunicationStates
{
    StartOfFrame = 0,
    FrameType,
    StatusCode,
    MessageID,
    SequenceNumber,
    Length,
    Payload,
    CRC,
    EndOfFrame
};

/// <summary>
///     Synchronisation states for the UART.
/// </summary>
enum UARTSyncStates
{
    Unknown = 0,
    Acknowledge,
    NegativeAcknowledge,
    Timeout,
    Reset,
    Error
};

struct BufferAndLength
{
    uint32_t BufferLength;
    uint8_t *Buffer;
};



class UART
{
private:
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
	///     This value should be set when the UART is configured.
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

	int bytesRemaining = 0;

	uint8_t *nextPayloadLocation = 0;

	struct ApplicationRequest messageHeader;

	/// <summary>
	///     Holds the current state of the communication state machine.
	/// </summary>
	enum CommunicationStates _communicationState = StartOfFrame;

	/// <summary>
	///     Number of consecutive occurrences of the start of frame character has been detected
	/// </summary>
	int _startOfFrameByteCount = 0;

	/// <summary>
	///     Type of frame being received.
	/// </summary>
	uint8_t _currentFrameType = 0;

	/// <summary>
	///     Number of bytes in the current data frame being received.
	/// </summary>
	uint8_t _framePayloadLength = 0;

	/// <summary>
	///     Payload of the frame currently being received.
	/// </summary>
	uint8_t *_framePayload = NULL;

	/// <summary>
	///     Location of the next payload byte in the buffer.
	/// </summary>
	uint8_t _framePayloadIndex = 0;

	/// <summary>
	///     Value of the checksum as read from the incoming data frame.
	/// </summary>
	uint32_t _originalChecksum = 0;

	/// <summary>
	///     Value of the calculated checksum from the progressive checksum calculation.
	/// </summary>
	uint32_t _calculatedChecksum = 0;

	/// <summary>
	///     This variable holds the byte number when putting together multi-byte
	///     values (i.e. 4 bytes in a UInt32 object).  this allows the original
	///     object to be re-hydrated from the UART byte stream.
	/// </summary>
	int _byteNumber = 0;

	/// <summary>
	///     Message ID of the data packets being received.
	/// </summary>
	uint32_t _messageID = 0;

	/// <summary>
	///     Sequence number of this frame in the total number of frames being sent.
	/// </summary>
	uint8_t _frameSequenceNumber = 0;

	/// <summary>
	///     Packet code returned by the transport packets.
	/// </summary>
	uint32_t _statusCode = 0;

	/// <summary>
	///     Buffer for the start of frame characters.
	/// </summary>
	char _startOfFrameCharacters[NUMBER_OF_START_OF_FRAME_CHARACTERS];

	/// <summary>
	///     Buffer for the end of frame characters.
	/// </summary>
	char _endOfFrameCharacters[NUMBER_OF_END_OF_FRAME_CHARACTERS];

	/// <summary>
	///     Current retry count.
	/// </summary>
	uint32_t _currentRetryCount = 0;

	/// <summary>
	///     Current maximum number of retries when sending a frame
	///     before the system declares an error condition.
	/// </summary>
	uint32_t _maximumRetryCount = UART_DEFAULT_MAXIMUM_RETRIES;

	/// <summary>
	///     Total number of bytes sent since the microcontroller was started.
	/// </summary>
	uint _totalTxBytes = 0;

	/// <summary>
	///     Total number of bytes received since the microcontroller was started.
	/// </summary>
	uint _totalRxBytes = 0;

	/// <summary>
	///     Total number of frames sent since the microcontroller was started.
	/// </summary>
	uint _totalFramesSent = 0;

	/// <summary>
	///     Total number of frames received since the microcontroller was started.
	/// </summary>
	uint _totalFramesReceived = 0;

	/// <summary>
	///     Number of Rx errors since the microcontroller was started.
	/// </summary>
	uint _totalRxErrors = 0;

	/// <summary>
	///     Number of Tx errors since the microcontroller was started.
	/// </summary>
	uint _totalTxErrors = 0;

	void ResetIncomingMessage(bool);
	uint32_t QueueMessage();
	uint32_t CalculateEncodedFrameLength(uint8_t *, uint8_t);
	int EncodeData(uint8_t *, uint32_t, uint8_t *);
	struct BufferAndLength *GenerateApplicationFrame(uint8_t, uint32_t, uint8_t, uint8_t *, uint32_t);
	struct BufferAndLength *GenerateTransportFrame(uint8_t *, uint32_t);
	void AcknowledgeFrame(uint8_t, uint32_t);
	void ProcessHeader(uint8_t *, uint8_t);
	void ProcessData(uint8_t *, uint8_t);
	void ProcessTransportData(uint8_t);
	void ProcessApplicationDataFrame(uint8_t, uint8_t *, uint8_t);
	enum UARTSyncStates SendFrame(struct BufferAndLength *, const char *);
	void DispatchRxMessage(struct ApplicationResponse *);

	void SetUartTransmitQueueHandle(QueueHandle_t);
	void SetUartRxQueueHandle(QueueHandle_t);
	void SetUartTransmitTaskHandle(TaskHandle_t);
	void SetUartReceiveTaskHandle(TaskHandle_t);


public:
	UART();
	~UART();
	void Setup();
//	void UARTTxTask(void *);
//	void UARTRxTask(void *);

	void DecodeSerialData(uint8_t *, int);
	void QueueResponse(struct ApplicationResponse *);
	void RxCleanup();
	void Send(struct ApplicationResponse *);

	QueueHandle_t GetUartRxQueueHandle() const;
	QueueHandle_t GetUartTransmitQueueHandle() const;
	TaskHandle_t GetUartReceiveTaskHandle() const;
	TaskHandle_t GetUartTransmitTaskHandle() const;
};

extern UART *uart;


#endif /* MAIN_UART_HPP_ */
