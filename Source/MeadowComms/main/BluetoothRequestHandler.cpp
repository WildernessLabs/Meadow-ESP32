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

#include "Encoders.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                   Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
bool BluetoothRequestHandler::_instantiated = false;

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
BluetoothRequestHandler::BluetoothRequestHandler()
{
    if (_instantiated)
    {
        throw new MultipleInstancesException();
    }
    else
    {
        _instantiated = true;
    }
}

/*
 *      Create a new BluetoothRequestHandler object.
 *
 *      @param messageDispatcher
 *          Message Dispatcher that can be used to send messages to the STM32.
 */
BluetoothRequestHandler::BluetoothRequestHandler(MessageDispatcher *messageDispatcher) : BluetoothRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/*
 *      Default destructor for the bluetooth class.
 */
BluetoothRequestHandler::~BluetoothRequestHandler()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                               Methods
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Setup the bluetooth system.
 */
void BluetoothRequestHandler::Setup()
{
    RequestHandlerBase::Setup();
    xTaskCreate(Task, _taskName, 1024 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
}

/*
 *      Dispatch bluetooth messages to the appropriate method.
 *
 *      @param request
 *          Bluetooth request being made by the STM32.
 */
void BluetoothRequestHandler::DispatchRequest(Message *request)
{
    ESP_LOGI(_taskName, "Message handling not implemented, message will be deleted");
    //
    //  Memory for the payload is allocated on the heap and it needs to be released.
    //
    if(request->Payload != NULL)
    {
        ESP_LOG_BUFFER_HEXDUMP(_taskName, request->Payload, request->PayloadLength, ESP_LOG_INFO);
        vPortFree(request->Payload);
    }
}
