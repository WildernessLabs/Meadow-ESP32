/*
 *      SystemRequestHandler.cpp
 *
 *      Implementation of the methods to provide the System Request Handler functionality.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include <string.h>
#include <stdlib.h>

#include "Encoders.hpp"
#include "SystemRequestHandler.hpp"
#include "MessageDispatcher.hpp"
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

/*
 *      Initialise a new System Request handler object.
 *
 *      @param messageDispatcher
 *          Message dispatcher object that will deal with and communication between the ESP32 and the STM32.
 */
SystemRequestHandler::SystemRequestHandler(MessageDispatcher *messageDispatcher) : SystemRequestHandler()
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
//    const size_t messageSize = sizeof(Esp32Messaging::Message);
    RequestHandlerBase::Setup();
//    _queueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
//    if (_queueHandle == NULL)
//    {
//        ESP_LOGI(__func__, "Cannot allocate System processing queue.");
//    }
    xTaskCreate(Task, _taskName, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
}

/*
 *      Implement the echo function.
 *
 *      Echo the message on the information console and then send a response back.
 *
 *      @param request
 *          Request from the system.
 */
void SystemRequestHandler::Echo(Esp32Messaging::Message *request)
{
    ESP_LOGI(__func__, "System received echo request: %s", (char *) request->Payload);
    vPortFree(request->Payload);
    const char *str = "Response from ESP32.";
    request->PayloadLength = strlen(str) + 1;
    request->Payload = (uint8_t *) pvPortMalloc(request->PayloadLength);
    strcpy((char *) request->Payload, str);
    _messageDispatcher->QueueMessageForStm32(request);
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
        case SystemFunction::Echo:
            Echo(request);
            break;
        default:
            ESP_LOGI(__func__, "Unknown System message received, function 0x%x", request->Function);
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
