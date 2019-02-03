/*
 * RequestHandler.cpp
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

#include "RequestHandlerBase.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                          Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Get the handle for the WiFi queue.
 */
QueueHandle_t RequestHandlerBase::GetQueueHandle()
{
    return(_queueHandle);
}

/*
 *      Set the handle for the WiFi queue.
 */
void RequestHandlerBase::SetQueueHandle(QueueHandle_t queueHandle)
{
    _queueHandle = queueHandle;
}

void RequestHandlerBase::SetTaskHandle(TaskHandle_t taskHandle)
{
    _taskHandle = taskHandle;
}

TaskHandle_t RequestHandlerBase::GetTaskHandle()
{
    return(_taskHandle);
}


