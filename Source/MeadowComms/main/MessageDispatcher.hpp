/*
 *      MessageDispatcher.hpp
 *
 *      Message dispatcher layer.  This object separates the need for the
 *      ESP32 interface objects to know about the hardware layer used to
 *      send messages between the ESP32 and the STM32.
 */

#ifndef _MESSAGE_DISPATCHER_HPP_
#define _MESSAGE_DISPATCHER_HPP_

#include "WiFiRequestHandler.hpp"
#include "SystemRequestHandler.hpp"
#include "BluetoothRequestHandler.hpp"
#include "MeshNetworkRequestHandler.hpp"
#include "TransportProviderBase.hpp"
#include "SharedEnums.hpp"

class MessageDispatcher
{
private:
    /*
     *      WiFi object that will process WiFi messages.
     */
    WiFiRequestHandler *_wifiRequestHandler = nullptr;

    /*
     *      Mesh Network object that will process Mesh Network messages.
     */
    MeshNetworkRequestHandler *_meshNetworkRequestHandler = nullptr;

    /*
     *      System object that will process System messages.
     */
    SystemRequestHandler *_systemRequestHandler = nullptr;

    /*
     *      Bluetooth object that will process Bluetooth messages.
     */
    BluetoothRequestHandler *_bluetoothRequestHandler = nullptr;

    /*
     *      WiFi object that will process WiFi messages.
     */
    TransportProviderBase *_transportProvider = nullptr;

    /*
     *      Constructor for the Message Dispatcher.
     */
    MessageDispatcher();

    /*
     *      Destructor for the Message Dispatcher.
     */
    ~MessageDispatcher();

public:
    /*
     *      Constructor for the Message Dispatcher.
     */
    MessageDispatcher(TransportProviderBase *);

    /*
     *      Queue a message for the ESP32 object.
     */
    bool QueueMessageForEsp32(Message *);

    /*
     *      Queue a message for the STM32.
     */
    bool QueueMessageForStm32(Message *);
};

#endif /* _MESSAGE_DISPATCHER_HPP_ */
