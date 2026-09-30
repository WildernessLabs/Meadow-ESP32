/**
 * @file Esp32Messaging.cpp
 * @author Mark Stevens
 * @brief Generic messaging constants, structures and definitions used in the messaging system.
 * @date 2019-04-26
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>

#include "Esp32Messaging.hpp"
#include "Logging.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Create an instance of a message on the heap.
 *
 *  @param messageType
 *      Type of message to send (see PacketType enum).
 *
 *  @param interface
 *      Interface on the ESP that this message should be routed to.
 *
 *  @param function
 *      Function to be called on the interface.
 *
 *  @param statusCode
 *      Status code to be returned by the function.
 *
 *  @param messageId
 *      Unique ID of thiS message.
 *
 *  @param payload
 *      Data required by the function (or being returned by the function).  Note
 *      that the payload should be on the heap.
 *
 *  @param payloadLength
 *      Size (in bytes) of the payload.
 *
 *  @returns
 *      Pointer to the newly created message.
 */
Message *Message::CreateOnHeap(const uint8_t messageType, const uint8_t interface, const uint32_t function, const uint32_t statusCode, const uint32_t messageId, uint8_t *payload, const uint32_t payloadLength)
{
    Message *result = static_cast<Message *>(pvPortMalloc(sizeof(Message)));

    if (result != NULL)
    {
        //
        //  pvPortMalloc runs no constructor, so every member not set below, such as
        //  PerformanceLoggingStart, would otherwise start as whatever the heap held.
        //
        memset(static_cast<void *>(result), 0, sizeof(Message));
        result->MessageType = messageType;
        result->Interface = interface;
        result->Function = function;
        result->StatusCode = statusCode;
        result->MessageID = messageId;
        result->PacketOffset = 0;
        result->PacketLength = 0;
        result->Payload = payload;
        result->PayloadLength = payloadLength;
    }

    return (result);
}

/**
 *  Create an instance of a message on the heap.
 *
 *  @param messageType
 *      Type of message to send (see PacketType enum).
 *
 *  @param interface
 *      Interface on the ESP that this message should be routed to.
 *
 *  @param function
 *      Function to be called on the interface.
 *
 *  @param statusCode
 *      Status code to be returned by the function.
 *
 *  @returns
 *      Pointer to the newly created message.
 */
Message *Message::CreateOnHeap(const uint8_t messageType, const uint8_t interface, const uint32_t function, const uint32_t statusCode)
{
    return (CreateOnHeap(messageType, interface, function, statusCode, 0, nullptr, 0));
}

/**
 *  @brief Create a copy of this message on the heap.
 *
 *  @param copyPayload
 *      Indicate if the payload should be copied as well or if we
 *      should just copy the header information.
 *
 *  @returns
 *      New copy of the message.
 */
Message *Message::CreateCopyOnHeap(const bool copyPayload)
{
    uint8_t *payload = nullptr;
    uint32_t payloadLength = 0;

    if (copyPayload && (Payload != nullptr) && (PayloadLength > 0))
    {
        payloadLength = PayloadLength;
        payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        if (payload == nullptr)
        {
            return (nullptr);
        }
        memcpy(static_cast<void *>(payload), static_cast<void *>(Payload), static_cast<size_t>(payloadLength));
    }
    Message *newMessage = CreateOnHeap(MessageType, Interface, Function, StatusCode, MessageID, payload, payloadLength);
    if (newMessage == nullptr)
    {
        //
        //  CreateOnHeap does not take ownership of payload on failure, so it is not
        //  otherwise reachable from here and has to be released before returning.
        //
        vPortFree(payload);
    }
    return (newMessage);
}

/**
 *  @brief Delete the heap storage associated with the payload.
 *
 *  On exit, the Payload will be set to a nullptr and the PayloadLength set to 0.
 */
void Message::DeletePayload()
{
    if (Payload != NULL)
    {
        vPortFree(Payload);
    }
    Payload = nullptr;
    PayloadLength = 0;
}

/**
 *  @brief Release the heap storage allocated for the message and any payload.
 *
 *  @param message
 *      Pointer to the message to be cleared, taken by reference so the caller's pointer
 *      is set to nullptr and cannot be freed or dereferenced a second time.
 */
void Message::DeleteMessage(Message *&message)
{
    if (message != nullptr)
    {
        if (message->Payload != nullptr)
        {
            vPortFree(message->Payload);
            message->Payload = nullptr;
        }
        vPortFree(message);
        message = nullptr;
    }
}
