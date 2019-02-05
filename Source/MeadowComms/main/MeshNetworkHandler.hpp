/*
 * MeshNetwork.hpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#ifndef MAIN_MESH_NETWORK_REQUEST_HANDLER_HPP_
#define MAIN_MESH_NETWORK_REQUEST_HANDLER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

#include "RequestHandlerBase.hpp"
#include "Messaging.h"

class MeshNetworkRequestHandler : public RequestHandlerBase
{
private:
    /*
     *      Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    bool _instantiated = false;

    /*
     *      Handle for the WiFi messages to be processed by the System task.
     */
    QueueHandle_t _queueHandle = NULL;

    /*
     *      Handle for the System request handler task.
     */
    TaskHandle_t _taskHandle = NULL;

    /*
     *      Name of the FreeRTOS task for the WiFi system.
     */
    const char *_taskName = "MeshNetworkTask";

    void DispatchRequest(ApplicationRequest *);
    static void Task(void *);

public:
    MeshNetworkRequestHandler();
    ~MeshNetworkRequestHandler();

    void Setup();
    QueueHandle_t GetQueueHandle();
    TaskHandle_t GetTaskHandle();
};

extern MeshNetworkRequestHandler *meshNetwork;

#endif /* MAIN_MESH_NETWORK_REQUEST_HANDLER_HPP_ */
