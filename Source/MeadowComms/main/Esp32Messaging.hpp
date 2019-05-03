/*
 *      Esp32Messaging.hpp
 *
 *      Generic messaging constants, structures and definitions used
 *      in the messaging system.
 */

#ifndef _ESP32_MESSAGING_HPP_
#define _ESP32_MESSAGING_HPP_

#include "freertos/FreeRTOS.h"

/*
 *      Messages between the ESP32 and the STM32 will be encoded and
 *      extracted through the message structure blow.
 */
struct Message
{
    uint8_t MessageType;
    uint8_t Interface;
    uint32_t Function;
    uint32_t StatusCode;
    uint32_t MessageID;
    uint32_t PayloadLength;
    uint8_t *Payload;

    /*
     *  Constructor for the Message class.
     */
    Message(uint8_t, uint8_t, uint32_t, uint32_t , uint32_t, uint8_t *, uint32_t);

    /*
     *  Default constructor for a Message structure.
     */
    Message();

    /*
     *      Make a copy of a message.
     */
    static Message *CopyMessage(Message *originalMessage, bool copyPayload);

    /*
     *      Create a new message on the heap with the specified values.
     */
    static Message *Create(uint8_t, uint8_t, uint32_t, uint32_t, uint32_t, uint8_t *, uint32_t);
};

const uint32_t HEADER_SIZE = 23;
const uint32_t CRC_OFFSET = 18;

#endif /* _ESP32_MESSAGING_HPP_ */
