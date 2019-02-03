/*
 * MeshNetwork.hpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#ifndef MAIN_MESHNETWORKHANDLER_HPP_
#define MAIN_MESHNETWORKHANDLER_HPP_

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

class MeshNetwork : public RequestHandlerBase
{
public:
    MeshNetwork();
    ~MeshNetwork();

    void Setup();

    const char *GetTaskName();
};

extern MeshNetwork *meshNetwork;

#endif /* MAIN_MESHNETWORKHANDLER_HPP_ */
