/*
 * RequestHandler.hpp
 *
 *  Created on: 3 Feb 2019
 *      Author: mark
 */

#ifndef MAIN_REQUESTHANDLERBASE_HPP_
#define MAIN_REQUESTHANDLERBASE_HPP_

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


class RequestHandlerBase
{
public:
    /*
     *      Handle for the WiFi messages to be processed by the WIFi task.
     */
    QueueHandle_t _queueHandle = NULL;

    /*
     *      Handle for the request handler task.
     */
    TaskHandle_t _taskHandle = NULL;

    /*
     * ----------------------------------------------------------------------------
     *
     *                          Getters and setters
     *
     * ----------------------------------------------------------------------------
     */

    /*
     *      Get the handle for the queue.
     */
    QueueHandle_t GetQueueHandle();

    /*
     *      Set the handle for the queue.
     */
    void SetQueueHandle(QueueHandle_t);

    /*
     *      Set the task handle.
     */
    void SetTaskHandle(TaskHandle_t);

    /*
     *      Set the task handle.
     */
    TaskHandle_t GetTaskHandle();

    /*
     *      Setup the various properties for correct functioning of the request handler.
     */
    virtual void Setup() = 0;
};


#endif /* MAIN_REQUESTHANDLERBASE_HPP_ */
