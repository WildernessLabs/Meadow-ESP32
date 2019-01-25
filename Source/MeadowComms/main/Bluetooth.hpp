/*
 * Bluetooth.hpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#ifndef MAIN_BLUETOOTH_HPP_
#define MAIN_BLUETOOTH_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdlib.h"

class Bluetooth
{
private:
    /*
     *      Handle for the Bluetooth processing queue.
     */
    QueueHandle_t BluetoothQueueHandle = NULL;

    /*
     *      Handle for the Bluetooth task.
     */
    TaskHandle_t xBluetoothTaskHandle = NULL;

    void SetQueueHandle(QueueHandle_t bluetoothQueueHandle);
    void SetTaskHandle(TaskHandle_t bluetoothTaskHandle);

public:
    Bluetooth();
    ~Bluetooth();

    void Setup();

    char *GetTaskName();
    QueueHandle_t GetQueueHandle();
    TaskHandle_t GetTaskHandle();
};

extern Bluetooth *bluetooth;


#endif /* MAIN_BLUETOOTH_HPP_ */
