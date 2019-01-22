/*
 *      Tasks.hpp
 *
 *
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
///     Handle for the WiFi task.
/// </summary>
extern TaskHandle_t xWiFiTaskHandle;

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
