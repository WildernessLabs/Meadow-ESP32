/*
 *      MeshNetworkRequestHandler.hpp
 *
 *      Mesh Network request handler object definition.
 */

#ifndef _MESH_NETWORK_REQUEST_HANDLER_HPP_
#define _MESH_NETWORK_REQUEST_HANDLER_HPP_

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
#include "Esp32Messaging.hpp"

/*
 *      Mesh network request handler
 */
class MeshNetworkRequestHandler : public RequestHandlerBase
{
private:
    /*
     *      Name of the FreeRTOS task for the WiFi system.
     */
    const char *_taskName = "MeshNetworkTask";

    /*
     *      Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /*
     *      This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Esp32Messaging::Message *);

    /*
     *      Private constructor(s) and destructor.
     */
    MeshNetworkRequestHandler();
    ~MeshNetworkRequestHandler();

public:
    /*
     *      Constructor for a Mesh Network request handler object.
     */
    MeshNetworkRequestHandler(MessageDispatcher *);

    /*
     *      Provide and additional setup operations not covered by the constructors.
     */
    void Setup();
};

#endif /* _MESH_NETWORK_REQUEST_HANDLER_HPP_ */
