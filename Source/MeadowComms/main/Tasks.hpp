/*
 * Tasks.hpp
 *
 *  Created on: 16 Jan 2019
 *      Author: markstevens
 */

#ifndef MAIN_TASKS_HPP_
#define MAIN_TASKS_HPP_

#ifdef __cplusplus

extern "C"
{

#endif


/// <summary>
///     Handle for the Bluetooth processing queue.
/// </summary>
extern QueueHandle_t BluetoothQueueHandle;

/// <summary>
///     Handle for the Bluetooth task.
/// </summary>
extern TaskHandle_t xBluetoothTaskHandle;

/// <summary>
///     Name of the Bluetooth task (used for logging).
/// </summary>
extern const char *BLUETOOTH_TASK_NAME;

/// <summary>
///     Handle for the Mesh network messages to be processed by the Mesh network task.
/// </summary>
extern QueueHandle_t MeshNetworkQueueHandle;

/// <summary>
///     Handle for the Mesh Network task.
/// </summary>
extern TaskHandle_t xMeshNetworkTaskHandle;

/// <summary>
///     Name of the Mesh network task (used for logging).
/// </summary>
extern const char *MESH_NETWORK_TASK_NAME;

/// <summary>
///     Handle for the WiFi messages to be processed by the WIFi task.
/// </summary>
// extern QueueHandle_t xWiFiQueueHandle;

/// <summary>
///     Handle for the WiFi task.
/// </summary>
extern TaskHandle_t xWiFiTaskHandle;

/// <summary>
///     Handle for the incoming messages on the UART.
/// </summary>
//extern QueueHandle_t UARTRxQueueHandle;

/// <summary>
///     Handle for the UART receive task.
/// </summary>
//extern TaskHandle_t xUARTReceiveTaskHandle;

//extern const char *UART_RX_TASK_NAME;

/// <summary>
///     Handle for the transmit handle for the UART.
/// </summary>
/// <remarks>
///     The UART transmit queue is used to hold data from tasks ready to
///     send the data to the UART.
/// </remarks>
//extern QueueHandle_t UARTTransmitQueueHandle;

/// <summary>
///     Handle for the UART transmit task.
/// </summary>
//extern TaskHandle_t xUARTTransmitTaskHandle;

/// <summary>
///     Name of the WiFi task (used for logging).
/// </summary>
extern const char *WIFI_TASK_NAME;

void BluetoothTask(void *);
void MeshNetworkTask(void *);
void WiFiTask(void *);
void UARTRxTask(void *);
void UARTTxTask(void *);

#ifdef __cplusplus

}

#endif


#endif /* MAIN_TASKS_HPP_ */
