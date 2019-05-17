/*
 *      Esp32Messaging.cpp
 *
 *      Generic messaging constants, structures and definitions used
 *      in the messaging system.
 */
#include <string.h>

#include "Esp32Messaging.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                         Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/*
 *  Default constructor for a Message structure.
 */
Message::Message()
{
    MessageType = 0;
    Interface = 0;
    Function = 0;
    StatusCode = 0;
    MessageID = 0;
    Payload = nullptr;
    PayloadLength = 0;
}

/*
 *  Constructor for the Message class.
 *
 *  @param messageType
 *      Type of message to send (see PacktType enum).
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
 *      Data required by the function (or being returned by the function).
 *      Note that the payload should be on the heap.
 *
 *  @param payloadLength
 *      Size (in bytes) of the payload.
 */
Message::Message(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode, uint32_t messageId, uint8_t *payload, uint32_t payloadLength)
{
    MessageType = messageType;
    Interface = interface;
    Function = function;
    StatusCode = statusCode;
    MessageID = messageId;
    Payload = payload;
    PayloadLength = payloadLength;
}

/*
 *  Create an instance of a message on the heap.
 *
 *  @param messageType
 *      Type of message to send (see PacktType enum).
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
 *      Data required by the function (or being returned by the function).
 *      Note that the payload should be on the heap.
 *
 *  @param payloadLength
 *      Size (in bytes) of the payload.
 */
Message *Message::CreateOnHeap(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode, uint32_t messageId, uint8_t *payload, uint32_t payloadLength)
{
    Message *result = (Message *) pvPortMalloc(sizeof(Message));

    memset(result, 0, sizeof(Message));
    result->MessageType = messageType;
    result->Interface = interface;
    result->Function = function;
    result->StatusCode = statusCode;
    result->MessageID = messageId;
    result->Payload = payload;
    result->PayloadLength = payloadLength;

    return(result);
}

/*
 *      Make a copy of a message.
 *
 *      @param originalMessage
 *          Original message to copy.
 *
 *      @param copyPayload
 *          Indicate if the payload should be copied as well or if we
 *          should just copy the header information.
 *
 *      @return
 *          New copy of the message.
 */
Message *Message::CopyMessage(Message *originalMessage, bool copyPayload)
{
    uint8_t *payload = nullptr;
    uint32_t payloadLength = 0;

    if (copyPayload)
    {
        payloadLength = originalMessage->PayloadLength;
        payload = (uint8_t *) pvPortMalloc(payloadLength);
        memcpy((void *) payload, (void *) originalMessage->Payload, (size_t) payloadLength);
    }
    Message *newMessage = CreateOnHeap(originalMessage->MessageType, originalMessage->Interface, originalMessage->Function, originalMessage->StatusCode, originalMessage->MessageID, payload, payloadLength);
    return(newMessage);
}

