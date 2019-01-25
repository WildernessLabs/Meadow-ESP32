/*
 *      Tasks.hpp
 *
 *      Task information for the RTOS tasks.
 */

#ifndef MAIN_TASKS_HPP_
#define MAIN_TASKS_HPP_

#ifdef __cplusplus

extern "C"
{

#endif

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
