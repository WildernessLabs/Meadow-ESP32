/*
 *      TransportProviderBase.hpp
 */

#ifndef _TRANSPORT_PROVIDER_BASE_HPP_
#define _TRANSPORT_PROVIDER_BASE_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include <string.h>
#include <stdlib.h>

#include "SharedEnums.hpp"
#include "Esp32Messaging.hpp"

//
//  Forward reference for the MessageDispatcher class.  This is needed
//  here to get around compiler errors if the MessageDispatcher.hpp
//  file is included.
//
class MessageDispatcher;

class TransportProviderBase
{
public:
    /*
     *      Setup the transport provider.
     */
    void Setup(MessageDispatcher *);

    /*
     *      Add the message given to the queue of messages waiting
     *      for transmission to the STM32.
     */
    bool QueueMessageForStm32(Esp32Messaging::Message *);

private:
    /*
     *      Maximum length of the message queue.
     */
    const uint32_t MAX_QUEUE_LENGTH = 10;

    /*
     *      Queue handle for the messages to be sent to the STM32.
     */
    QueueHandle_t _queueHandle = NULL;

protected:
    /*
     *      Default constructor.
     */
    TransportProviderBase();

    /*
     *      Default destructor.
     */
    ~TransportProviderBase();

    /*
     *      Message dispatcher object used to send messages to the
     *      various interfaces on the ESP32.
     */
    MessageDispatcher *_messageDispatcher = nullptr;
};

#endif /* _TRANSPORT_PROVIDER_BASE_HPP_ */
