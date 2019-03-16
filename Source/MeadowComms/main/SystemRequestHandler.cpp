/*
 * SystemRequestHandler.cpp
 *
 *  Created on: 3 Feb 2019
 *      Author: mark
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include <string.h>
#include <stdlib.h>

#include "Encoders.hpp"
#include "SystemRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                   Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
bool SystemRequestHandler::_instantiated = false;

/*
 * ----------------------------------------------------------------------------
 *
 *                      Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Default constructor for the system request handler class.
 */
SystemRequestHandler::SystemRequestHandler()
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

SystemRequestHandler::SystemRequestHandler(MessageDispatcher *messageDispatcher)
{
    _messageDispatcher = messageDispatcher;
}

/*
 *      Default destructor for the system request handler class.
 */
SystemRequestHandler::~SystemRequestHandler()
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
 *      Setup the system request handler.
 */
void SystemRequestHandler::Setup()
{
    /*
     *      Now create the objects necessary for the System task.
     */
    const size_t messageSize = sizeof(Esp32Messaging::Message);
    _queueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (_queueHandle == NULL)
    {
        ESP_LOGI(__func__, "Cannot allocate System processing queue.");
    }
    xTaskCreate(Task, _taskName, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
    _instantiated = true;
}

/*
 *      Dispatch the System request to the appropriate method in the class.
 *
 *      @param request
 *          Request to be processed.
 */
void SystemRequestHandler::DispatchRequest(Esp32Messaging::Message *request)
{
    switch (request->Function)
    {
        default:
            ESP_LOGI(__func__, "Unknown System message received, function %d", request->Function);
            if (request->Payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(__func__, request->Payload, request->PayloadLength, ESP_LOG_INFO);
            }
            break;
    }
    //
    //  Memory for the payload is allocated on the heap and it needs to be released.
    //
    if (request->Payload != NULL)
    {
        vPortFree(request->Payload);
    }
}
