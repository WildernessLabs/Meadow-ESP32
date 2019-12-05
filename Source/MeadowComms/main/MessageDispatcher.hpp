/*
 *  MessageDispatcher.hpp
 *
 *  Message dispatcher layer.  This object separates the need for the
 *  ESP32 interface objects to know about the hardware layer used to
 *  send messages between the ESP32 and the STM32.
 */

#ifndef _MESSAGE_DISPATCHER_HPP_
#define _MESSAGE_DISPATCHER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "WiFiRequestHandler.hpp"
#include "SystemRequestHandler.hpp"
#include "BluetoothRequestHandler.hpp"
#include "MeshNetworkRequestHandler.hpp"
#include "TransportProviderBase.hpp"
#include "SharedEnums.hpp"
#include "Esp32Messaging.hpp"

/*
 *  Forward declarations required by the compiler.
 */
class WiFiRequestHandler;
class MeshNetworkRequestHandler;
class SystemRequestHandler;
class BluetoothRequestHandler;

/*
 *  @brief Message dispatcher class.
 *
 *  This class acts as a broker between the ESP32 and the STM32
 *  message transmit / receive mechanisms.
 */
class MessageDispatcher
{
private:
    /**
     *  This mask is applied to message IDs to make sure that they are all
     *  above 0x7fffffff.  This identifies them as originating from the
     *  ESP32 rather than the STM32.
     */
    const uint32_t ESP32_MESSAGE_ID_MASK = 0x80000000;

    /**
     *  WiFi object that will process WiFi messages.
     */
    WiFiRequestHandler *_wifiRequestHandler = nullptr;

    /**
     *  Mesh Network object that will process Mesh Network messages.
     */
    MeshNetworkRequestHandler *_meshNetworkRequestHandler = nullptr;

    /**
     *  System object that will process System messages.
     */
    SystemRequestHandler *_systemRequestHandler = nullptr;

    /**
     *  Bluetooth object that will process Bluetooth messages.
     */
    BluetoothRequestHandler *_bluetoothRequestHandler = nullptr;

    /**
     *  Hardware transport provider object that is connected to
     *  the STM32.
     */
    TransportProviderBase *_transportProvider = nullptr;

    /**
     *  This mutex ensures that one one task can ask for a new message ID
     *  at any time.
     */
    SemaphoreHandle_t _messageIdMutex;

    /**
     *  Last message ID that was granted by the system.  This always defaults
     *  to 0 as it will be incremented before a new ID is granted.
     */
    uint32_t _lastMessageId = 0;

    /*
     *  Constructor for the Message Dispatcher.
     */
    MessageDispatcher();

    /*
     *  Destructor for the Message Dispatcher.
     */
    ~MessageDispatcher();

public:
    /*
     *  Constructor for the Message Dispatcher.
     */
    explicit MessageDispatcher(TransportProviderBase *);

    /*
     *  Queue a message for the ESP32.
     */
    bool QueueMessageForEsp32(Message *);

    /*
     *  Queue a message for the STM32.
     */
    bool QueueMessageForStm32(Message *);

    /**
     *  Get the next message ID.
     */
    uint32_t GetNextMessageID();
};

#endif /* _MESSAGE_DISPATCHER_HPP_ */
