/*
 * SystemRequestHandler.hpp
 *
 *  Created on: 3 Feb 2019
 *      Author: mark
 */

#ifndef MAIN_SYSTEMREQUESTHANDLER_HPP_
#define MAIN_SYSTEMREQUESTHANDLER_HPP_

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

class SystemRequestHandler : public RequestHandlerBase
{
private:
    /*
     *      Name of the FreeRTOS task for the System system.
     */
    const char *_taskName = "SystemTask";

    /*
     *      Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    bool _instantiated = false;

    /*
     *      FreeRTOS task for processing requests.
     */
    static void Task(void *);
    void DispatchRequest(ApplicationRequest *);

public:
    /*
     *      Constructor(s) and destructor.
     */
    SystemRequestHandler();
    ~SystemRequestHandler();

    /*
     *      Public methods.
     */
    void Setup();
};

extern SystemRequestHandler *systemRequestHandler;

#endif /* MAIN_SYSTEMREQUESTHANDLER_HPP_ */
