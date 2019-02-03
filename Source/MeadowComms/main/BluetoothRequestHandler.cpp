/*
 *  Bluetooth.cpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#include "BluetoothRequestHandler.hpp"
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
#include "Tasks.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                      Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Default constructor for the bluetooth class.
 */
Bluetooth::Bluetooth()
{
}

/*
 *      Default destructor for the bluetooth class.
 */
Bluetooth::~Bluetooth()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                          Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/*
 *
 */
const char *Bluetooth::GetTaskName()
{
    return("BluetoothTask");
}

/*
 *      Setup the bluetooth system.
 */
void Bluetooth::Setup()
{
    const size_t messageSize = sizeof(ApplicationRequest);
    SetQueueHandle(xQueueCreate(MAX_QUEUE_LENGTH, messageSize));
    if (GetQueueHandle() == NULL)
    {
        ESP_LOGI(__func__, "Cannot allocate Bluetooth processing queue.");
    }
    xTaskCreate(BluetoothTask, bluetooth->GetTaskName(), 1024 * 2, NULL, configMAX_PRIORITIES - 1, &_taskHandle);
}
