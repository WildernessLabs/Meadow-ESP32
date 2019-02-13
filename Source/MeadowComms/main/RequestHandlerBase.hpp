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
private:
    /*
     *      Handle for the System messages to be processed by the System task.
     */
    QueueHandle_t _queueHandle = NULL;

    /*
     *      Handle for the System request handler task.
     */
    TaskHandle_t _taskHandle = NULL;


protected:
    void SetQueueHandle(QueueHandle_t);
    TaskHandle_t GetTaskHandle();
    void SetTaskHandle(TaskHandle_t);

public:
    /*
     *      Setup the various properties for correct functioning of the request handler.
     */
    virtual void Setup() = 0;
    virtual ~RequestHandlerBase() { };
    QueueHandle_t GetQueueHandle();
};


#endif /* MAIN_REQUEST_HANDLER_BASE_HPP_ */
