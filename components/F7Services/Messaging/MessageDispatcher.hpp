/**
 * @file MessageDispatcher.hpp
 * @author Mark Stevens
 * @brief Message dispatcher layer implementation.
 * @date 2019-02-22
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "Esp32Messaging.hpp"
#include "IMessageDispatcher.hpp"
#include "TransportProviderBase.hpp"

/*
 *  Forward declarations required by the compiler.
 */
class WiFiRequestHandler;
class SystemRequestHandler;
class BluetoothRequestHandler;

/**
 *  @brief Message dispatcher class.
 *
 *  This class acts as a broker between the ESP32 and the STM32
 *  message transmit / receive mechanisms.
 */
class MessageDispatcher : public IMessageDispatcher
{
private:
    /**
     *  @brief Mask for the message IDs for outbound messages.
     *
     *  This mask is cleared in the message IDs the ESP32 grants, so they are
     *  all below 0x80000000.  The STM32 sets it in the IDs it grants, so the
     *  top bit says which end of the link a message originated from.
     */
    const uint32_t ESP32_MESSAGE_ID_MASK = 0x80000000;

    /**
     *  @brief WiFi object that will process WiFi messages.
     */
    WiFiRequestHandler *_wifiRequestHandler = nullptr;

    /**
     *  @brief System object that will process System messages.
     */
    SystemRequestHandler *_systemRequestHandler = nullptr;

    /**
     *  @brief Bluetooth object that will process Bluetooth messages.
     */
    BluetoothRequestHandler *_bluetoothRequestHandler = nullptr;

    /**
     *  @brief Hardware transport provider object that is connected to
     *  the STM32.
     */
    TransportProviderBase *_transportProvider = nullptr;

    /**
     *  @brief This mutex ensures that one one task can ask for a new message ID
     *  at any time.
     */
    SemaphoreHandle_t _messageIdMutex;

    /**
     *  @brief Set once all of the request handlers exist.
     *
     *  The transport provider's task starts taking requests from the STM32 as soon as it is
     *  set up, which has to happen before the handlers are created as they can queue
     *  messages for the STM32 while they start.  Requests wait on this rather than finding
     *  no handler to give them to (issue #168).  Setting and waiting on the bit also makes
     *  the handler pointers visible to the transport provider's task.
     */
    EventGroupHandle_t _handlersReady;

    /**
     *  @brief Bit in _handlersReady set once the request handlers have been created.
     */
    static constexpr EventBits_t HANDLERS_READY_BIT = BIT0;

    /**
     *  @brief Last message ID that was granted by the system.  This always defaults
     *  to 0 as it will be incremented before a new ID is granted.
     */
    uint32_t _lastMessageId = 0;

    /**
     *  @brief Constructor for the Message Dispatcher.
     */
    MessageDispatcher();

    /**
     *  @brief Destructor for the Message Dispatcher.
     */
    ~MessageDispatcher();

public:
    /**
     *  @brief Name of the message dispatcher task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Constructor for the Message Dispatcher.
     */
    explicit MessageDispatcher(TransportProviderBase * const transportProvider);

    /**
     *  @brief Queue a message for the ESP32.
     */
    bool QueueMessageForEsp32(Message *request) override;

    /**
     *  @brief Queue a message for the STM32.
     */
    bool QueueMessageForStm32(Message *response) override;

    /**
     *  @brief Get the next message ID.
     */
    uint32_t GetNextMessageID() override;
};
