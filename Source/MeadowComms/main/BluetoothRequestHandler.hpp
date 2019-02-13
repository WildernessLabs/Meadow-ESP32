/*
 * Bluetooth.hpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#ifndef MAIN_BLUETOOTH_REQUEST_HANDLER_HPP_
#define MAIN_BLUETOOTH_REQUEST_HANDLER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdlib.h"

#include "Messaging.h"
#include "RequestHandlerBase.hpp"

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
    bool _instantiated = false;

    void DispatchRequest(ApplicationRequest *);
    static void Task(void *);

public:
    /*
     *      Constructor(s) and destructor.
     */
    BluetoothRequestHandler();
    ~BluetoothRequestHandler();

    /*
     *      Public methods.
     */
    void Setup();
};

extern BluetoothRequestHandler *bluetooth;


#endif /* MAIN_BLUETOOTH_REQUEST_HANDLER_HPP_ */
