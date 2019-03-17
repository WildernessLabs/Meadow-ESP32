/*
 *      RequestHandler.hpp
 *
 *      Base class for the request handlers.
 */

#ifndef _REQUEST_HANDLER_BASE_HPP_
#define _REQUEST_HANDLER_BASE_HPP_

#include "freertos/FreeRTOS.h"

#include "Esp32Messaging.hpp"

//
//  Forward reference for the MessageDispatcher class.  This is needed
//  here to get around compiler errors if the MessageDispatcher.hpp
//  file is included.
//
class MessageDispatcher;

class RequestHandlerBase
{
private:
    /*
     *      Dispatch messages.
     *
     *      This method is unique to each specific interface.
     */
    virtual void DispatchRequest(Message *) = 0;

protected:
    /*
     *      Handle for the System messages to be processed by the System task.
     */
    QueueHandle_t _queueHandle = nullptr;

    /*
     *      Handle for the System request handler task.
     */
    TaskHandle_t _taskHandle = nullptr;

    /*
     *      Object that will be used to send messages to the STM32
     *      from the ESP32.
     */
    MessageDispatcher *_messageDispatcher = nullptr;

    /*
     *      Task that will be used to retrieve and dispatch
     *      messages to the appropriate method.
     */
    static void Task(void *);

    /*
     *      Constructor for the RequestHandlerBase class.
     */
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
    RequestHandlerBase(MessageDispatcher *);

    /*
     *      Setup the various properties for correct functioning of the request handler.
     */
    void Setup();

    /*
     *      Add the message to the queue of messages waiting to be
     *      processed by the interface object.
     */
    bool QueueEsp32Message(Message *);
};


#endif /* _REQUEST_HANDLER_BASE_HPP_ */
