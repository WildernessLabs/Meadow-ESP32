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

class TransportProviderBase
{
public:
    /*
     *      Setup the transport provider.
     */
    void Setup();

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
     *      Function to call when a new message is received.
     */
    //QueueMessageForStm32FunctionPointer _queueMessageForStm32FunctionPointer;

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
};

#endif /* _TRANSPORT_PROVIDER_BASE_HPP_ */
