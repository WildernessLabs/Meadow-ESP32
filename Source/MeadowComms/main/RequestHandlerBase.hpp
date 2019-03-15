/*
 *      RequestHandler.hpp
 *
 *      Base class for the request handlers.
 */

#ifndef _REQUEST_HANDLER_BASE_HPP_
#define _REQUEST_HANDLER_BASE_HPP_

#include "freertos/FreeRTOS.h"

#include "Esp32Messaging.hpp"

class RequestHandlerBase
{
private:
    /*
     *      Dispatch messages.
     *
     *      This method is unique to each specific interface.
     */
    virtual void DispatchRequest(Esp32Messaging::Message *) = 0;

protected:
    /*
     *      Get the task handle for the
     */
    TaskHandle_t GetTaskHandle();

    /*
     *      Handle for the System messages to be processed by the System task.
     */
    QueueHandle_t _queueHandle = NULL;

    /*
     *      Handle for the System request handler task.
     */
    TaskHandle_t _taskHandle = NULL;

    /*
     *      Task that will be used to retrieve and dispatch
     *      messages to the appropriate method.
     */
    void Task(void *);

    RequestHandlerBase();

    /*
     *      Destructor for the RequestHandlerBase class.
     */
    ~RequestHandlerBase();


public:
    /*
     *  Maximum length of a message queue.
     */
    const uint32_t MAX_QUEUE_LENGTH = 10;

    /*
     *      Constructor for the RequestHandlerBase class.
     */
    RequestHandlerBase(Esp32Messaging::MessageReceivedFunctionPointer);

    /*
     *      Setup the various properties for correct functioning of the request handler.
     */
    void Setup();
};


#endif /* _REQUEST_HANDLER_BASE_HPP_ */
