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
};

const uint32_t HEADER_SIZE = 23;
const uint32_t CRC_OFFSET = 18;

#endif /* _ESP32_MESSAGING_HPP_ */
