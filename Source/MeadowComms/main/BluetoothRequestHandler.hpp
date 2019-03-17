/*
 *      BluetoothRequestHandler.hpp
 *
 *      Bluetooth request handler will deal with incoming bluetooth requests.
 */

#ifndef _BLUETOOTH_REQUEST_HANDLER_HPP_
#define _BLUETOOTH_REQUEST_HANDLER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdlib.h"

#include "RequestHandlerBase.hpp"
#include "Esp32Messaging.hpp"

/*
 *      Bluetooth request handler object.
 */
class BluetoothRequestHandler : public RequestHandlerBase
{
private:
    /*
     *      Name of the FreeRTOS task for the Bluetooth system.
     */
    const char *_taskName = "BluetoothTask";

    /*
     *      Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /*
     *      This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *);

    /*
     *      Private constructor and destructor.
     */
    BluetoothRequestHandler();
    ~BluetoothRequestHandler();

public:
    /*
     *      Construct a new Bluetooth request handler object with reference to a
     *      Message Dispatcher object.
     */
    BluetoothRequestHandler(MessageDispatcher *);

    /*
     *      Provide and additional setup operations not covered by the constructors.
     */
    void Setup();
};

#endif /* _BLUETOOTH_REQUEST_HANDLER_HPP_ */
