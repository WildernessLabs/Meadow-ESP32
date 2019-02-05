/*
 *      RequestHandler.hpp
 *
 *      Base class for the request handlers.
 */

#ifndef MAIN_REQUEST_HANDLER_BASE_HPP_
#define MAIN_REQUEST_HANDLER_BASE_HPP_

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
protected:

public:
    /*
     *      Setup the various properties for correct functioning of the request handler.
     */
    virtual void Setup() = 0;
    virtual QueueHandle_t GetQueueHandle() = 0;
    virtual TaskHandle_t GetTaskHandle() = 0;
};


#endif /* MAIN_REQUEST_HANDLER_BASE_HPP_ */
