//
//  Functions, macros etc for the ESP32 UART message send / receive for Meadow.
//
#pragma once

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
//  Dafault maximum number of time the UART will retry sending a message.
//
#define UART_DEFAULT_MAXIMUM_RETRIES    (3)

//
//  Handle for the queue dealing with the incoming messages.
//
extern QueueHandle_t UARTRxQueueHandle;

//
//  Handle for the UART receive task.
//
extern TaskHandle_t xUARTReceiveTaskHandle;

//
//  Handle for the UART transmit task.
//
extern TaskHandle_t xUARTTransmitTaskHandle;

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
    Reset
};

struct BufferAndLength
{
    uint32_t BufferLength;
    uint8_t *Buffer;
};

//
//  Function prototypes
//
extern void SetupUART();
void UARTTxTask(void *);
void UARTRxTask(void *);

void QueueUARTResponse(uint8_t, uint32_t, uint32_t, uint8_t *, uint8_t);
