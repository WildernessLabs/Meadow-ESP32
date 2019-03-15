/*
 *      Esp32Messaging.hpp
 *
 *      Generic messaging constants, structures and definitions used
 *      in the messaging system.
 */

#ifndef _ESP32_MESSAGING_HPP_
#define _ESP32_MESSAGING_HPP_

#include "freertos/FreeRTOS.h"
#include "SharedEnums.hpp"

namespace Esp32Messaging
{
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
        uint32_t Crc;
        uint8_t *Payload;
    };

    static const uint32_t MESSAGE_SIZE = 23;
    static const uint32_t CRC_OFFSET = 18;

    typedef void (*MessageReceivedFunctionPointer)(Message *);
    //typedef StatusCodes::StatusCodes (MessageDispatcher::*QueueMessageForStm32FunctionPointer(Message *));
}


#endif /* _ESP32_MESSAGING_HPP_ */
