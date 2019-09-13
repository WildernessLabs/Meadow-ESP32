/*
 *  TransportProviderBase.hpp
 *
 *  This class is a base class for the hardware level data transmit / receive
 *  hardware used to connect the ESP32 and the STM32 chips.
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

/**
 *
 */
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

    /**
     *  Bit that will be used to indicate that a the STM32 has just
     *  finished sending a message to the ESP32.
     */
    static const uint32_t STM32_INTERRUPT_NOTIFICATION_BIT = 0x00000001;

private:
    /**
     *  Maximum length of the message queue.
     */
    const uint32_t MAX_QUEUE_LENGTH = 10;

protected:
    /**
     *  Message dispatcher object used to send messages to the
     *  various interfaces on the ESP32.
     */
    MessageDispatcher *_messageDispatcher = nullptr;

    /**
     *  Queue handle for the messages to be sent to the STM32.
     */
    static QueueHandle_t _outboundMessageQueueHandle;

    /**
     *  Mutex for the outbound message queue.
     */
    static SemaphoreHandle_t _outboundMessageQueueMutex;

    /**
     *  Task handle for the FreeRTOS task that will deal with the
     *  process of sending and receiving messages between the ESP32
     *  and the STM32 chips.
     */
    static TaskHandle_t _transportProviderTaskHandle;

    /*
     *  Default constructor.
     */
    TransportProviderBase();

    /*
     *  Default destructor.
     */
    ~TransportProviderBase();

    /*
     *  Get the number of messages in the outbound message queue.
     */
    int OutboundMessageQueueLength();

    /*
     *  Get the next message from the outbound message queue.
     */
    Message *GetOutboundMessage();
};

#endif /* _TRANSPORT_PROVIDER_BASE_HPP_ */
