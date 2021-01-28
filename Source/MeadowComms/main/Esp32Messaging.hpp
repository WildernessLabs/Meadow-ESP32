/*
 *  Esp32Messaging.hpp
 *
 *  Generic messaging constants, structures and definitions used
 *  in the messaging system.
 */

#ifndef _ESP32_MESSAGING_HPP_
#define _ESP32_MESSAGING_HPP_

#include "freertos/FreeRTOS.h"

/*
 *  Messages between the ESP32 and the STM32 will be encoded and
 *  extracted through the message structure blow.
 */
struct Message
{
    /**
     *  Type of message.
     */
    uint8_t MessageType;

    /**
     *  Interface that this message is destined for.
     */
    uint8_t Interface;

    /**
     *  Function (on the interface) to be executed.
     */
    uint32_t Function;

    /**
     *  Status code (for returning messages) from the function.
     */
    uint32_t StatusCode;

    /**
     *  Unique ID of this message.
     */
    uint32_t MessageID;

    /**
     *  Pointer to the payload data to be processed (or returned from) the function.
     */
    uint8_t *Payload;

    /**
     *  Number of bytes in the payload.
     */
    uint32_t PayloadLength;

#if defined(PERFORMANCE_LOGGING)
    int64_t PerformanceLoggingStart;
#endif


    /**
     *  Size of an encoded message header (in bytes).
     */
    static const uint32_t HEADER_SIZE = 23;

    /**
     *  Offset of the CRC in an encoded message header.
     */
    static const uint32_t CRC_OFFSET = 18;

    /**
     *  Message ID used to indicate an invalid (or unknown) message ID.
     */
    static const uint32_t INVALID_MESSAGE_ID = 0xffffffff;

    /**
     *  Constructor for the Message class.
     */
    Message(uint8_t, uint8_t, uint32_t, uint32_t , uint32_t, uint8_t *, uint32_t);

    /**
     *  Constructor for the Message class.
     */
    Message(uint8_t, uint8_t, uint32_t, uint32_t);

    /**
     *  Default constructor for a Message structure.
     */
    Message();

    /**
     *  Copy constructor.
     */
    Message(const Message &);

    /**
     *  Make a copy of the message.
     */
    Message *CreateCopyOnHeap(bool);

    /**
     *  Create a new message on the heap with the specified values.
     */
    static Message *CreateOnHeap(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode, uint32_t messageId, uint8_t *payload, uint32_t payloadLength);

    /**
     *  Create a new message on the heap with the specified values.
     */
    static Message *CreateOnHeap(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode);

    /**
     *  Delete the payload and free up heap storage.
     */
    void DeletePayload();

    /**
     *  Delete the message and payload releassing heap storage.
     */
    static void DeleteMessage(Message *);
};

#endif /* _ESP32_MESSAGING_HPP_ */
