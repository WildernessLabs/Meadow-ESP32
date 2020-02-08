/*
 *  SystemRequestHandler.cpp
 *
 *  Implementation of the methods to provide the System Request Handler functionality.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <string.h>
#include <stdlib.h>

#include "Encoders.hpp"
#include "SystemRequestHandler.hpp"
#include "MessageDispatcher.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
/**
 *  Determine if this class is instantiated already (i.e. this should be a singleton).
 */
bool SystemRequestHandler::_instantiated = false;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Default constructor for the system request handler class.
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

/**
 *  Initialise a new System Request handler object.
 *
 *  @param messageDispatcher
 *      Message dispatcher object that will deal with and communication between the ESP32 and the STM32.
 */
SystemRequestHandler::SystemRequestHandler(MessageDispatcher *messageDispatcher) : SystemRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/**
 *  Default destructor for the system request handler class.
 */
SystemRequestHandler::~SystemRequestHandler()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Setup the system request handler.
 */
void SystemRequestHandler::Setup()
{
    RequestHandlerBase::Setup();
    xTaskCreate(Task, _taskName, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
}

/**
 *  @brief Get the configuration for the system.
 *
 *  @param request
 *      Message requesting the system configuration.
 */
void SystemRequestHandler::GetConfiguration(Message *request)
{
    ESP_LOGI(__func__, "GetConfiguration not implemented");
    //  TODO: Implement the GetConfiguration method.
}

/**
 *  @brief Set the configuration for the system.
 *
 *  @param request
 *      Message containing the system configuration.
 */
void SystemRequestHandler::SetConfiguration(Message *request)
{
    ESP_LOGI(__func__, "SetConfiguration not implemented");
    //  TODO: Implement the SetConfiguration method.
}

/**
 *  @brief Put the system into deep sleep.
 *
 *  This is one of the few messages that will not send a response to the
 *  STM32.  The ESP32 will simply go to sleep.
 *
 *  @param request
 *      Message requesting the ESP32 got to sleep.
 */
void SystemRequestHandler::DeepSleep(Message *request)
{
    ESP_LOGI(__func__, "DeepSleep not implemented");
    //  TODO: Implement the DeepSleep method.
}

/**
 *  @brief Get the voltage reading from the battery charging circuit.
 *
 *  @param request
 *      Message requesting the battery level.
 */
void SystemRequestHandler::BatteryLevel(Message *request)
{
    ESP_LOGI(__func__, "BatteryLevel not implemented");
    //  TODO: Implement the BatteryLevel method.
}

/**
 *  Dispatch the System request to the appropriate method in the class.
 *
 *  @param request
 *      Request to be processed.
 */
void SystemRequestHandler::DispatchRequest(Message *request)
{
    switch (request->Function)
    {
        case SystemFunction::GetConfiguration:
            break;
        case SystemFunction::SetConfiguration:
            break;
        case SystemFunction::DeepSleep:
            break;
        case SystemFunction::BatteryLevel:
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
