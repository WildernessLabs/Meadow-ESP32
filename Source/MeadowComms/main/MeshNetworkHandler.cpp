/*
 * MeshNetwork.cpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */
#include "MeshNetworkHandler.hpp"

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
 *      Default constructor for the mesh network.
 */
MeshNetwork::MeshNetwork()
{
}

/*
 *      Default destructor for the mesh network class.
 */
MeshNetwork::~MeshNetwork()
{
}

/*
 *      Setup the mesh network.
 */
void MeshNetwork::Setup()
{
    const size_t messageSize = sizeof(ApplicationRequest);
    SetQueueHandle(xQueueCreate(MAX_QUEUE_LENGTH, messageSize));
    if (GetQueueHandle() == NULL)
    {
        ESP_LOGI(GetTaskName(), "Cannot allocate Mesh network queue.");
    }
    xTaskCreate(MeshNetworkTask, GetTaskName(), 1024 * 2, NULL, configMAX_PRIORITIES - 1, &_taskHandle);
}
