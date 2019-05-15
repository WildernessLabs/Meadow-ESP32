/*
 *      TransportProviderBase.hpp
 *
 *      This class is a base class for the hardware level data transmit / receive
 *      hardware used to connect the ESP32 and the STM32 chips.
 */

#ifndef _TRANSPORT_PROVIDER_BASE_HPP_
#define _TRANSPORT_PROVIDER_BASE_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Esp32Messaging.hpp"

/*
 *  Forward reference for the MessageDispatcher class.  This is needed
 *  here to get around compiler errors if the MessageDispatcher.hpp
 *  file is included.
 */
class MessageDispatcher;

class TransportProviderBase
{
public:
    /*
     *  Setup the transport provider.
     */
    void Setup(MessageDispatcher *);

    /*
     *  Add the message given to the queue of messages waiting
     *  for transmission to the STM32.
     */
    bool QueueMessageForStm32(Message *);

    static const uint32_t STM32_INTERRUPT_NOTIFICATION_BIT = 0x00000001;
    static const uint32_t OUTBOUND_MESSAGE_QUEUE_NOTIFICATION_BIT = 0x00000002;

private:
    /*
     *  Maximum length of the message queue.
     */
    const uint32_t MAX_QUEUE_LENGTH = 10;

protected:
    /*
     *  Default constructor.
     */
    TransportProviderBase();

    /*
     *  Default destructor.
     */
    ~TransportProviderBase();

    /*
     *  Message dispatcher object used to send messages to the
     *  various interfaces on the ESP32.
     */
    MessageDispatcher *_messageDispatcher = nullptr;

    /*
     *  Queue handle for the messages to be sent to the STM32.
     */
    static QueueHandle_t _outboundMessageQueueHandle;

    /*
     *  Mutex for the outbound message queue.
     */
    static SemaphoreHandle_t _outboundMessageQueueMutex;

    static TaskHandle_t _transportProviderTaskHandle;
};

#endif /* _TRANSPORT_PROVIDER_BASE_HPP_ */
