/**
 * @file RequestHandlerBase.hpp
 * @author Mark Stevens
 * @brief Base class for all request handlers.
 * @date 2019-02-03
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"

#include "Esp32Messaging.hpp"

//
//  Forward reference for the MessageDispatcher class.  This is needed
//  here to get around compiler errors if the MessageDispatcher.hpp
//  file is included.
//
class IMessageDispatcher;

/**
 *  @brief Base class for any request handlers.
 */
class RequestHandlerBase
{
private:
    /**
     *  @brief Dispatch messages.
     *
     *  This method is unique to each specific interface.
     */
    virtual void DispatchRequest(Message *) = 0;

protected:
    /**
     *  @brief Handle for the System messages to be processed by the System task.
     */
    QueueHandle_t _queueHandle = nullptr;

    /**
     *  @brief Handle for the System request handler task.
     */
    TaskHandle_t _taskHandle = nullptr;

    /**
     *  @brief Object that will be used to send messages to the STM32 from the ESP32.
     */
    static IMessageDispatcher *_messageDispatcher;

    /**
     *  @brief Task that will be used to retrieve and dispatch messages to the appropriate method.
     */
    static void Task(void *pvParameters);

    /**
     *  @brief Constructor for the RequestHandlerBase class.
     */
    RequestHandlerBase();

    /**
     *  @brief Destructor for the RequestHandlerBase class.
     */
    ~RequestHandlerBase();

    /**
     *  @brief Create a new event message and add to the outbound message queue.
     */
    void RaiseEvent(const uint8_t interface, const uint32_t eventType, const uint32_t statusCode, uint8_t *payload, const uint32_t payloadLength);

    /**
     *  @brief Create a new event message and add to the outbound message queue.
     */
    void RaiseEvent(const uint8_t interface, const uint32_t eventType, const uint32_t statusCode);

    /**
     *  @brief Create a new event message and add to the outbound message queue.
     */
    void RaiseEvent(const uint8_t interface, const uint32_t eventType);

    /**
     * @brief Send an IntegerResponse to the STM32.
     *
     * @param interface - The interface to sending the response.
     * @param eventType - The event type of the response.
     * @param statusCode - The status code of the response.
     * @param result - Value to send.
     */
    void SendIntegerResult(const uint8_t interface, const uint32_t eventType, const uint32_t statusCode, const uint32_t value);

public:
    /**
     *  @brief Name of this component / task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Maximum length of a message queue.
     */
    const uint32_t MAX_QUEUE_LENGTH = 10;

    /**
     *  @brief Constructor for the RequestHandlerBase class.
     */
    explicit RequestHandlerBase(IMessageDispatcher *message);

    /**
     *  @brief Setup the various properties for correct functioning of the request handler.
     */
    void Setup();

    /**
     *  @brief Add the message to the queue of messages waiting to be processed by the interface object.
     */
    bool QueueEsp32Message(Message *message);
};
