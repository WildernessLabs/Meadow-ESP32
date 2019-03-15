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
#include "Esp32Messaging.hpp"

class MeshNetworkRequestHandler : public RequestHandlerBase
{
private:
    /*
     *      Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    bool _instantiated = false;

    /*
     *      Name of the FreeRTOS task for the WiFi system.
     */
    const char *_taskName = "MeshNetworkTask";

    void DispatchRequest(Esp32Messaging::Message *);
    static void Task(void *);

public:
    MeshNetworkRequestHandler();
    ~MeshNetworkRequestHandler();

    void Setup();
};

extern MeshNetworkRequestHandler *meshNetwork;

#endif /* MAIN_MESH_NETWORK_REQUEST_HANDLER_HPP_ */
