/*
 *  MeshNetworkRequestHandler.cpp
 *
 *  Provide the implementation of the Mesh Networking functions.
 */
#include "MeshNetworkRequestHandler.hpp"
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
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Determine if this class is instantiated already (i.e. this should be a singleton).
 */
bool MeshNetworkRequestHandler::_instantiated = false;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Default constructor for the mesh network.
 */
MeshNetworkRequestHandler::MeshNetworkRequestHandler()
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
 *  Create a new Mesh Network handler object.
 *
 *  @param messageDispatcher
 *      Message dispatcher that will allow communication with the STM32.
 */
MeshNetworkRequestHandler::MeshNetworkRequestHandler(MessageDispatcher *messageDispatcher) : MeshNetworkRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/**
 *  Default destructor for the mesh network class.
 */
MeshNetworkRequestHandler::~MeshNetworkRequestHandler()
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
 *  Setup the mesh network.
 */
void MeshNetworkRequestHandler::Setup()
{
    RequestHandlerBase::Setup();
    xTaskCreate(Task, _taskName, 1024 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
}

/**
 *  Dispatch the message
 */
void MeshNetworkRequestHandler::DispatchRequest(Message *request)
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
