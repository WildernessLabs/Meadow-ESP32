/*
 * MeshNetwork.hpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#ifndef MAIN_MESHNETWORK_HPP_
#define MAIN_MESHNETWORK_HPP_

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

class MeshNetwork
{
private:
    /*
     *      Handle for the Mesh network messages to be processed by the Mesh network task.
     */
    QueueHandle_t MeshNetworkQueueHandle = NULL;

    /*
     *      Handle for the Mesh Network task.
     */
    TaskHandle_t xMeshNetworkTaskHandle = NULL;

    void SetQueueHandle(QueueHandle_t meshNetworkQueueHandle);
    void SetTaskHandle(TaskHandle_t meshNetworkTaskHandle = NULL);

public:
    MeshNetwork();
    ~MeshNetwork();

    void Setup();

    char *GetTaskName();
    QueueHandle_t GetQueueHandle();
    TaskHandle_t GetTaskHandle();
};

extern MeshNetwork *meshNetwork;

#endif /* MAIN_MESHNETWORK_HPP_ */
