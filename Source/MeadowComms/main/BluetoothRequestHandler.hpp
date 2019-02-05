/*
 * Bluetooth.hpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#ifndef MAIN_BLUETOOTHREQUESTHANDLER_HPP_
#define MAIN_BLUETOOTHREQUESTHANDLER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdlib.h"

#include "Messaging.h"
#include "RequestHandlerBase.hpp"

class Bluetooth : public RequestHandlerBase
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

    /*
     *      Handle for the Bluetooth message queue to be processed by the Bluetooth task.
     */
    QueueHandle_t _queueHandle = NULL;

    /*
     *      Handle for the Bluetooth request handler task.
     */
    TaskHandle_t _taskHandle = NULL;

    void DispatchRequest(ApplicationRequest *);
    static void Task(void *);

public:
    /*
     *      Constructor(s) and destructor.
     */
    Bluetooth();
    ~Bluetooth();

    /*
     *      Public methods.
     */
    void Setup();
    const QueueHandle_t GetQueueHandle();
    const TaskHandle_t GetTaskHandle();
};

extern Bluetooth *bluetooth;


#endif /* MAIN_BLUETOOTHREQUESTHANDLER_HPP_ */
