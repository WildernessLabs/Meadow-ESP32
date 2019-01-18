/*
 *  Tasks.cpp
 *
 *	Create the tasks for the RTOS.
 */

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "esp_log.h"

#include "Tasks.hpp"
#include "WiFi.hpp"
#include  "Messaging.h"
#include "UART.hpp"

/// <summary>
///     Handle for the Bluetooth processing queue.
/// </summary>
QueueHandle_t BluetoothQueueHandle = NULL;

/// <summary>
///     Handle for the Bluetooth task.
/// </summary>
TaskHandle_t xBluetoothTaskHandle = NULL;

/// <summary>
///     Name of the Bluetooth task (used for logging).
/// </summary>
const char *BLUETOOTH_TASK_NAME = const_cast<const char *> ("BluetoothTask");

/// <summary>
///     Handle for the Mesh network messages to be processed by the Mesh network task.
/// </summary>
QueueHandle_t MeshNetworkQueueHandle = NULL;

/// <summary>
///     Handle for the Mesh Network task.
/// </summary>
TaskHandle_t xMeshNetworkTaskHandle = NULL;

/// <summary>
///     Name of the Mesh network task (used for logging).
/// </summary>
const char *MESH_NETWORK_TASK_NAME = const_cast<const char *> ("MeshNetworkTask");

QueueHandle_t xWiFiQueueHandle = NULL;

// <summary>
//  	Handle for the WiFi task.
// </summary>
TaskHandle_t xWiFiTaskHandle = NULL;

// <summary>
// 		Name of the WiFi Task.
// </summary>
const char *WIFI_TASK_NAME = const_cast<const char *> ("WiFiTask");

/// <summary>
///     Handle for the incoming messages on the UART.
/// </summary>
QueueHandle_t UARTRxQueueHandle = NULL;

/// <summary>
///     Handle for the UART receive task.
/// </summary>
TaskHandle_t xUARTReceiveTaskHandle = NULL;

const char *UART_RX_TASK_NAME = const_cast<const char *> ("UARTRxTask");

/// <summary>
///     Handle for the transmit handle for the UART.
/// </summary>
/// <remarks>
///     The UART transmit queue is used to hold data from tasks ready to
///     send the data to the UART.
/// </remarks>
QueueHandle_t UARTTransmitQueueHandle = NULL;

/// <summary>
///     Handle for the UART transmit task.
/// </summary>
TaskHandle_t xUARTTransmitTaskHandle = NULL;

const char *UART_TX_TASK_NAME = const_cast<const char *> ("UARTTxTask");



/// <summary>
///     Task to process the Bluetooth messages.
/// <summary>
void BluetoothTask(void *pvParameters)
{
    while (true)
    {
        struct ApplicationRequest message;
        BaseType_t result = xQueueReceive(BluetoothQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            ESP_LOGI(BLUETOOTH_TASK_NAME, "Message handling not implemented, message will be deleted");
            //
            //  Memory for the payload is allocated on the heap and it needs to be released.
            //
            if(message.payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(BLUETOOTH_TASK_NAME, message.payload, message.payloadLength, ESP_LOG_INFO);
                vPortFree(message.payload);
            }
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xBluetoothTaskHandle);
}


/// <summary>
///     Task to process the Mesh network messages.
/// <summary>
void MeshNetworkTask(void *pvParameters)
{
    while (true)
    {
        struct ApplicationRequest message;
        BaseType_t result = xQueueReceive(MeshNetworkQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            ESP_LOGI(MESH_NETWORK_TASK_NAME, "Message handling not implemented, message will be deleted");
            //
            //  Memory for the payload is allocated on the heap and it needs to be released.
            //
            if(message.payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(MESH_NETWORK_TASK_NAME, message.payload, message.payloadLength, ESP_LOG_INFO);
                vPortFree(message.payload);
            }
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xMeshNetworkTaskHandle);
}

/// <summary>
///     Task to process the WiFi messages.
/// <summary>
void WiFiTask(void *pvParameters)
{
    struct ApplicationRequest message;
    QueueHandle_t wifiQueueHandle = wifi->GetQueueHandle();
    while (true)
    {
        BaseType_t result = xQueueReceive(wifiQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
        	wifi->DispatchRequest(&message);
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xWiFiTaskHandle);
}

/// <summary>
///     Task to process the WiFi messages.
/// <summary>
void UARTTxTask(void *pvParameters)
{
	QueueHandle_t txQueueHandle = uart->GetUartTransmitQueueHandle();
	TaskHandle_t txTaskHandle = uart->GetUartTransmitTaskHandle();
    while (true)
    {
        struct ApplicationResponse message;

        BaseType_t result = xQueueReceive(txQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
        	uart->Send(&message);
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(txTaskHandle);
}

/// <summary>
///     UART data received interrupt task.
/// </summary>
void UARTRxTask(void *pvParameters)
{
    uart_event_t event;
    QueueHandle_t uartRxHandle = uart->GetUartRxQueueHandle();
    TaskHandle_t uartRxTaskHandle = uart->GetUartReceiveTaskHandle();
    uint8_t* data = (uint8_t *) pvPortMalloc(RX_BUF_SIZE);

    while (true)
    {
        int dataLeftToRead = 0;
        int amountToRead = 0;
        if (xQueueReceive(uartRxHandle, (void *) &event, portMAX_DELAY))
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
                        uart->DecodeSerialData(data, amountToRead);
                        dataLeftToRead -= amountToRead;
                    }
                    break;
                case UART_BUFFER_FULL:
                case UART_FIFO_OVF:
                    //
                    //  Flush the input buffer in the case of a full ring buffer or a FIFO overflow.
                    //
                    uart_flush_input(STM32_UART_NUMBER);
                    xQueueReset(uartRxHandle);
                    break;
                default:
                    uart_flush_input(STM32_UART_NUMBER);
                    xQueueReset(uartRxHandle);
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
    uart->RxCleanup();
    vTaskDelete(uartRxTaskHandle);
}
