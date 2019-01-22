/*
 *  UART.hpp
 *
 *  UART Class for the ESP32.
 *
 *  This header file defines the UART class which encapsulates the UART
 *  functionality for the ESP32.  This class allows the ESP32 to
 *  communicate with the STM32 over a serial port.
 */
#ifndef MAIN_UART_HPP_
#define MAIN_UART_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

#include "WiFi.hpp"
#include "Tasks.hpp"
#include "Messaging.h"
#include "ErrorCodes.h"
#include "Encoders.hpp"

/*
 *      BufferAndLength holds a buffer and the amount of data in
 *      the buffer (restricted to 8-bits).
 */
struct BufferAndLength
{
    /*
     *      Amount of data in the buffer.
     */
    uint32_t BufferLength;

    /*
     *      Pointer to the buffer.
     */
    uint8_t *Buffer;
};

/*
 *      Implementation UART communication between the ESP32 and the STM32.
 */
class UART
{
private:
    /*
     *      Communication states for the UART.
     */
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

    /*
     *      Synchronisation states for the UART.
     */
    enum UARTSyncStates
    {
        Unknown = 0,
        Acknowledge,
        NegativeAcknowledge,
        Timeout,
        Reset,
        Error
    };

    /*
     *  A repeating pattern of this character indicates a start of message.
     */
    static const int START_OF_FRAME_CHARACTER = 0xaa;

    /*
     *      Character marking the end of a data frame.
     */
    static const int END_OF_FRAME_CHARACTER = 0x55;

    /*
     *      The filler character is used to send a sequence of Start of
     *      Frame characters in the binary data.
     */
    static const int PADDING_CHARACTER = 0x52;

    /*
     *      Number of characters to be used in the pattern matching.
     */
    static const int NUMBER_OF_START_OF_FRAME_CHARACTERS = 3;

    /*
     *      Number of end of frame characters.
     */
    static const int NUMBER_OF_END_OF_FRAME_CHARACTERS = 1;

    /*
     *      Pin used to send data to the STM32.
     */
    const int TX_PIN = (GPIO_NUM_4);

    /*
     *      Pin used to receive data from the STM32.
     */
    const int RX_PIN = (GPIO_NUM_5);

    /*
     *      Length of the UART queue.
     */
    const int  QUEUE_LENGTH = 10;

    /*
     *      Maximum number of unencoded bytes in a frame.
     */
    const int MAXIMUM_FRAME_LENGTH = 255;

    /*
     *      Default maximum number of time the UART will retry sending a message.
     */
    const int DEFAULT_MAXIMUM_RETRIES = 3;

    /*
     *      Handle for the UART Tx queue.
     */
    QueueHandle_t _xUARTTxQueueHandle = NULL;

    /*
     *      Name of the Tx task.
     */
    const char *UART_TX_TASK_NAME = const_cast<const char *> ("UARTTxTask");

    /*
     *      Handle for the Tx task.
     */
    TaskHandle_t _xUARTTxTaskHandle = NULL;

    /*
     *      Handle for the UART Rx queue.
     */
    QueueHandle_t _xUARTRxQueueHandle = NULL;

    /*
     *      Name of the Rx Task.
     */
    const char *UART_RX_TASK_NAME = const_cast<const char *> ("UARTRxTask");

    /*
     *      Handle for the Rx task.
     */
    TaskHandle_t _xUARTRxTaskHandle = NULL;

    /*
     *      Handle to the queue used to sync the UART messages.
     *
     *      The sync queue is used by the transport message handling methods to
     *      communicate with the Tx and Rx tasks to provide a measure of reliability
     *      and recovery.
     */
    QueueHandle_t _xUARTSyncQueueHandle = NULL;

    /*
     *      This is the maximum number of milliseconds between sending a frame and a
     *      response being received by the ESP32.  This is a variable as it will depend
     *      upon the baud rate and the size of a frame.
     *
     *      This value should be set when the UART is configured.
     */
    TickType_t _uartMaximumSendResponsePeriod = 0;

    /*
     *      Mutex to ensure that only one request to access the UART can be
     *      processed at any time.
     *
     *      The UARTTXTask and the ACK/NAK processing could need access
     *      the UART and this mutex is used to synchronise the access.
     */
    SemaphoreHandle_t _xUARTTxMutex = NULL;

    /*
     *      Number of payload bytes remaining.
     */
    int _bytesRemaining = 0;

    /*
     *      Byte used to store the next byte received as part of a message payload.
     */
    uint8_t *_nextPayloadLocation = 0;

    /*
     *      Message header for the message currently being received.
     */
    ApplicationRequest _messageHeader;

    /*
     *      Holds the current state of the communication state machine.
     */
    CommunicationStates _communicationState = StartOfFrame;

    /*
     *      Number of consecutive occurrences of the start of frame character has been detected
     */
    int _startOfFrameByteCount = 0;

    /*
     *      Type of frame being received.
     */
    uint8_t _currentFrameType = 0;

    /*
     *      Number of bytes in the current data frame being received.
     */
    uint8_t _framePayloadLength = 0;

    /*
     *      Payload of the frame currently being received.
     */
    uint8_t *_framePayload = NULL;

    /*
     *      Location of the next payload byte in the buffer.
     */
    uint8_t _framePayloadIndex = 0;

    /*
     *      Value of the checksum as read from the incoming data frame.
     */
    uint32_t _originalChecksum = 0;

    /*
     *      Value of the calculated checksum from the progressive checksum calculation.
     */
    uint32_t _calculatedChecksum = 0;

    /*
     *      This variable holds the byte number when putting together multi-byte values
     *      (i.e. 4 bytes in a UInt32 object).  this allows the original object to be
     *      re-hydrated from the UART byte stream.
     */
    int _byteNumber = 0;

    /*
     *      Message ID of the data packets being received.
     */
    uint32_t _messageID = 0;

    /*
     *      Sequence number of this frame in the total number of frames being sent.
     */
    uint8_t _frameSequenceNumber = 0;

    /*
     *      Packet code returned by the transport packets.
     */
    uint32_t _statusCode = 0;

    /*
     *      Buffer for the start of frame characters.
     */
    char _startOfFrameCharacters[NUMBER_OF_START_OF_FRAME_CHARACTERS];

    /*
     *      Buffer for the end of frame characters.
     */
    char _endOfFrameCharacters[NUMBER_OF_END_OF_FRAME_CHARACTERS];

    /*
     *      Current retry count.
     */
    uint32_t _currentRetryCount = 0;

    /*
     *      Current maximum number of retries when sending a
     *      frame before the system declares an error condition.
     */
    uint32_t _maximumRetryCount = DEFAULT_MAXIMUM_RETRIES;

    /*
     *      Total number of bytes sent since the microcontroller was started.
     */
    uint _totalTxBytes = 0;

    /*
     *      Total number of bytes received since the microcontroller was started.
     */
    uint _totalRxBytes = 0;

    /*
     *      Total number of frames sent since the microcontroller was started.
     */
    uint _totalFramesSent = 0;

    /*
     *      Total number of frames received since the microcontroller was started.
     */
    uint _totalFramesReceived = 0;

    /*
     *      Number of Rx errors since the microcontroller was started.
     */
    uint _totalRxErrors = 0;

    /*
     *      Number of Tx errors since the microcontroller was started.
     */
    uint _totalTxErrors = 0;

    void ResetIncomingMessage(bool);
    uint32_t QueueMessage();
    uint32_t CalculateEncodedFrameLength(uint8_t *, uint8_t);
    int EncodeData(uint8_t *, uint32_t, uint8_t *);
    BufferAndLength *GenerateApplicationFrame(uint8_t, uint32_t, uint8_t, uint8_t *, uint32_t);
    BufferAndLength *GenerateTransportFrame(uint8_t *, uint32_t);
    void AcknowledgeFrame(uint8_t, uint32_t);
    void ProcessHeader(uint8_t *, uint8_t);
    void ProcessData(uint8_t *, uint8_t);
    void ProcessTransportData(uint8_t);
    void ProcessApplicationDataFrame(uint8_t, uint8_t *, uint8_t);
    UARTSyncStates SendFrame(BufferAndLength *, const char *);
    void DispatchRxMessage(ApplicationResponse *);

    void SetUartTransmitQueueHandle(QueueHandle_t);
    void SetUartRxQueueHandle(QueueHandle_t);
    void SetUartTransmitTaskHandle(TaskHandle_t);
    void SetUartReceiveTaskHandle(TaskHandle_t);

public:
    /*
     *
     *      UART used for communication with the STM32.
     */
    const uart_port_t STM32_UART_PORT_NUMBER = UART_NUM_1;
    /*
     *      Number of bytes in the Rx buffer.
     */
    static const int RX_BUFFER_SIZE = 1024;
    /*
     *      Number of bytes in the Rx buffer.
     */
    static const int TX_BUFFER_SIZE = 1024;


    UART();
    ~UART();

    void Setup();
    void DecodeSerialData(uint8_t *, int);
    void QueueResponse(ApplicationResponse *);
    void RxCleanup();
    void Send(ApplicationResponse *);

    QueueHandle_t GetUartRxQueueHandle() const;
    QueueHandle_t GetUartTransmitQueueHandle() const;
    TaskHandle_t GetUartReceiveTaskHandle() const;
    TaskHandle_t GetUartTransmitTaskHandle() const;
};

extern UART *uart;

#endif /* MAIN_UART_HPP_ */
