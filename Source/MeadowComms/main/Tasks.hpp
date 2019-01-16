/*
 * Tasks.hpp
 *
 *  Created on: 16 Jan 2019
 *      Author: markstevens
 */

#ifndef MAIN_TASKS_HPP_
#define MAIN_TASKS_HPP_

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
extern char *BLUETOOTH_TASK_NAME;


#ifdef __cplusplus

extern "C"
{

#endif


void BluetoothTask(void *);


#ifdef __cplusplus

}

#endif


#endif /* MAIN_TASKS_HPP_ */
