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
#include "Messaging.h"
#include "SharedEnums.hpp"

class MessageDispatcher
{
private:
    /*
     *      Message header for the message currently being received.
     */
    ApplicationRequest _messageHeader;

    /*
     *      Number of payload bytes remaining.
     */
    int _bytesRemaining = 0;

    /*
     *      Byte used to store the next byte received as part of a message payload.
     */
    uint8_t *_nextPayloadLocation = NULL;

    WiFiRequestHandler *_wifiRequestHandler = NULL;
    MeshNetworkRequestHandler *_meshNetworkRequestHandler = NULL;
    SystemRequestHandler *_systemRequestHandler = NULL;
    BluetoothRequestHandler *_bluetoothRequestHandler = NULL;
    TransportProviderBase *_transportProvider = NULL;

    MessageDispatcher();
    ~MessageDispatcher();

    void ResetIncomingMessage(bool);
    StatusCodes::StatusCodes ProcessData(uint8_t *, uint8_t);
    StatusCodes::StatusCodes ProcessHeader(uint8_t *, uint8_t);

public:
    MessageDispatcher(TransportProviderBase *);

    BluetoothRequestHandler* GetBluetoothRequestHandler();
    void SetBluetoothRequestHandler(BluetoothRequestHandler* bluetoothRequestHandler);
    MeshNetworkRequestHandler* GetMeshNetworkRequestHandler();
    void SetMeshNetworkRequestHandler(MeshNetworkRequestHandler* meshNetworkRequestHandler);
    SystemRequestHandler* GetSystemRequestHandler();
    void SetSystemRequestHandler(SystemRequestHandler* systemRequestHandler);
    WiFiRequestHandler* GetWifiRequestHandler();
    void SetWifiRequestHandler(WiFiRequestHandler* wifiRequestHandler);

    StatusCodes::StatusCodes QueueMessage(ApplicationRequest *);
    StatusCodes::StatusCodes ProcessApplicationDataFrame(uint8_t, uint8_t *, uint8_t);
};

#endif /* _MESSAGE_DISPATCHER_HPP_ */
