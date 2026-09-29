/**
 * @file TransportProviderBase.hpp
 * @author Mark Stevens
 * @brief Base class for the hardware level data transmit/receive hardware used to connect the ESP32 and the STM32 chips.
 * @date 2019-02-22
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include "Esp32Messaging.hpp"

/*
 *  Forward reference for the MessageDispatcher class.  This is needed
 *  here to get around compiler errors if the MessageDispatcher.hpp
 *  file is included.
 */
class MessageDispatcher;

/**
 *  @brief Base class for the message system transport provider.
 */
class TransportProviderBase
{
public:
    /**
     *  @brief Setup the transport provider.
     */
    void Setup(MessageDispatcher *messageDispatcher);

    /**
     *  @brief Add the message given to the queue of messages waiting
     *  for transmission to the STM32.
     *
     *  @note This method takes ownership of the message.  The message is deleted
     *        if it cannot be queued, so the caller must not use it afterwards.
     */
    bool QueueMessageForStm32(Message *message);

    /**
     *  @brief Bit that will be used to indicate that a the STM32 has just
     *  finished sending a message to the ESP32.
     */
    static const uint32_t STM32_INTERRUPT_NOTIFICATION_BIT = 0x00000001;

    /**
     * @brief Control message that can be sent to the STM32.
     */
    enum ControlMessageType {
        Reset = 1,
        MessageWaiting,
        Ready,
        SpiInterfaceReady,
        Busy,
    };

    ControlMessageType GetControlSignalMessage();

    void SetControlSignalMessage(const ControlMessageType);

    TickType_t GetControlSignalDelay();

    void SetControlSignalDelay(const TickType_t);

private:
    /**
     *  @brief Maximum length of the message queue.
     */
    const uint32_t MAX_QUEUE_LENGTH = 10;

    /**
     *  @brief Number of times an attempt is made to queue a message for the STM32.
     */
    static const uint32_t QUEUE_ATTEMPTS = 3;

    /**
     *  @brief Delay between the attempts to queue a message for the STM32.
     */
    static const TickType_t QUEUE_ATTEMPT_DELAY = pdMS_TO_TICKS(100);

    /**
     * @brief Control message type to send to the STM32 in the
     */
    ControlMessageType _controlSignalMessage = ControlMessageType::Ready;

    /**
     * @brief Delay for the control signal task.
     */
    TickType_t _controlSignalDelay = pdMS_TO_TICKS(2000);

protected:
    /**
     *  @brief Message dispatcher object used to send messages to the
     *  various interfaces on the ESP32.
     */
    MessageDispatcher *_messageDispatcher = nullptr;

    /**
     *  @brief Queue handle for the messages to be sent to the STM32.
     */
    static QueueHandle_t _outboundMessageQueueHandle;

    /**
     *  @brief Mutex for the outbound message queue.
     */
    static SemaphoreHandle_t _outboundMessageQueueMutex;

    /**
     *  @brief Task handle for the FreeRTOS task that will deal with the
     *  process of sending and receiving messages between the ESP32
     *  and the STM32 chips.
     */
    static TaskHandle_t _transportProviderTaskHandle;

    /**
     *  @brief Default constructor.
     */
    TransportProviderBase();

    /**
     *  @brief Default destructor.
     */
    ~TransportProviderBase();

    /**
     *  @brief Get the number of messages in the outbound message queue.
     */
    int OutboundMessageQueueLength();

    /**
     *  @brief Get the next message from the outbound message queue.
     */
    Message *GetOutboundMessage();

    /**
     *  @brief Send a control message to the STM32.
     */
    void SendControlMessageToStm32(const ControlMessageType);

    /**
     *  @brief Method used for debugging, this will dump the message at the head
     *  of the message queue as LOGI informaiton.
     */
    void DumpMessageAtHeadOfQueue();

public:
    /**
     *  @brief Name of this component / task.
     */
    static const char *COMPONENT_NAME;
};
