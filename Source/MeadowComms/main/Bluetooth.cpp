/*
 *  Bluetooth.cpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

#include "Messaging.h"
#include "Bluetooth.hpp"
#include "Tasks.hpp"

Bluetooth::Bluetooth()
{
}

Bluetooth::~Bluetooth()
{
}

QueueHandle_t Bluetooth::GetQueueHandle()
{
    return BluetoothQueueHandle;
}

void Bluetooth::SetQueueHandle(QueueHandle_t bluetoothQueueHandle)
{
    BluetoothQueueHandle = bluetoothQueueHandle;
}

TaskHandle_t Bluetooth::GetTaskHandle()
{
    return xBluetoothTaskHandle;
}

void Bluetooth::SetTaskHandle(TaskHandle_t bluetoothTaskHandle)
{
    xBluetoothTaskHandle = bluetoothTaskHandle;
}

char *Bluetooth::GetTaskName()
{
    return("BluetoothTask");
}

void Bluetooth::Setup()
{
    const size_t messageSize = sizeof(ApplicationRequest);
    BluetoothQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (BluetoothQueueHandle == NULL)
    {
        ESP_LOGI(__func__, "Cannot allocate Bluetooth processing queue.");
    }
    xTaskCreate(BluetoothTask, bluetooth->GetTaskName(), 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
}
