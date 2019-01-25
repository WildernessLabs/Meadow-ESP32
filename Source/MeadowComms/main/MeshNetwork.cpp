/*
 * MeshNetwork.cpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */
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
#include "MeshNetwork.hpp"
#include "Tasks.hpp"

MeshNetwork::MeshNetwork()
{
}

MeshNetwork::~MeshNetwork()
{
}

QueueHandle_t MeshNetwork::GetQueueHandle()
{
    return MeshNetworkQueueHandle;
}

void MeshNetwork::SetQueueHandle(QueueHandle_t meshNetworkQueueHandle)
{
    MeshNetworkQueueHandle = meshNetworkQueueHandle;
}

TaskHandle_t MeshNetwork::GetTaskHandle()
{
    return xMeshNetworkTaskHandle;
}

void MeshNetwork::SetTaskHandle(TaskHandle_t meshNetworkTaskHandle)
{
    xMeshNetworkTaskHandle = meshNetworkTaskHandle;
}

char *MeshNetwork::GetTaskName()
{
    return("MeshNetworkTask");
}

void MeshNetwork::Setup()
{
    const size_t messageSize = sizeof(ApplicationRequest);
    MeshNetworkQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, messageSize);
    if (MeshNetworkQueueHandle == NULL)
    {
        ESP_LOGI(GetTaskName(), "Cannot allocate Mesh network queue.");
    }
    xTaskCreate(MeshNetworkTask, GetTaskName(), 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xMeshNetworkTaskHandle);
}
