/*
 * MessageDispatcher.hpp
 *
 *  Created on: 15 Feb 2019
 *      Author: mark
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
    WiFiRequestHandler *_wifiRequestHandler;
    MeshNetworkRequestHandler *_meshNetworkRequestHandler;
    SystemRequestHandler *_systemRequestHandler;
    BluetoothRequestHandler *_bluetoothRequestHandler;
    TransportProviderBase *_transportProvider;

    MessageDispatcher();
    ~MessageDispatcher();

public:
    MessageDispatcher(TransportProviderBase *);

    void Setup();

    StatusCodes::StatusCodes QueueMessage(Esp32Messaging::Message *);
};


#endif /* _MESSAGE_DISPATCHER_HPP_ */
