/*
 *      SystemRequestHandler.hpp
 *
 *      System message request handler
 */

#ifndef _SYSTEMREQUESTHANDLER_HPP_
#define _SYSTEMREQUESTHANDLER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include <string.h>
#include <stdlib.h>

#include "RequestHandlerBase.hpp"

/*
 *      Object to deal with system requests such as configuration settings etc.
 */
class SystemRequestHandler : public RequestHandlerBase
{
private:
    /*
     *      Name of the FreeRTOS task for the System task.
     */
    const char *_taskName = "SystemTask";

    /*
     *      Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /*
     *      This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Esp32Messaging::Message *);

    /*
     *      Constructor(s) and destructor.
     */
    SystemRequestHandler();
    ~SystemRequestHandler();

public:
    /*
     *
     */
    SystemRequestHandler(MessageDispatcher *);

    /*
     *      Public methods.
     */
    void Setup();
};

#endif /* _SYSTEMREQUESTHANDLER_HPP_ */
