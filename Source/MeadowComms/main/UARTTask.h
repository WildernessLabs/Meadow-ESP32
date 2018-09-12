//
//  Functions, macros etc for the ESP32 UART message send / receive for Meadow.
//
#pragma once

//
//  Number of bytes in the receive buffer.
//
static const int RX_BUF_SIZE = 1024;

//
//  Which pins are used UART.
//
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

//
//  Handle for the UART receive task.
//
extern TaskHandle_t xUARTReceiveTaskHandle;

//
//  Handle for the UART transmit task.
//
extern TaskHandle_t xUARTTransmitTaskHandle;

//
//  Friendly name for the receive task (used for logging).
//
extern char *UART_RX_TASK_NAME;

//
//  Friendly name for the transmit task (used for logging).
//
extern char *UART_TX_TASK_NAME;

//
//  Function prototypes
//
extern void SetupUART();
void UARTTxTask(void *);
void UARTRxTask(void *);