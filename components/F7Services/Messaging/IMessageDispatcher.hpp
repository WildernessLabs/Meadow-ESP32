/**
 * @file IMessageDispatcher.hpp
 * @author Mark Stevens
 * @brief Message dispatcher layer. This object separates the need for the ESP32 interface objects to know about the hardware layer used to send messages between the ESP32 and the STM32.
 * @date 2023-02-01
 *
 * @copyright Copyright (c) 2023 Wilderness Labs
 */

#pragma once

#include "sdkconfig.h"

#include "Esp32Messaging.hpp"

/**
 *  @brief Message dispatcher class.
 *
 *  This class acts as a broker between the ESP32 and the STM32
 *  message transmit / receive mechanisms.
 */
class IMessageDispatcher
{
public:
    /**
     *  @brief Queue a message for the ESP32.
     */
    virtual bool QueueMessageForEsp32(Message *request) = 0;

    /**
     *  @brief Queue a message for the STM32.
     */
    virtual bool QueueMessageForStm32(Message *response) = 0;

    /**
     *  @brief Get the next message ID.
     */
    virtual uint32_t GetNextMessageID() = 0;
};
