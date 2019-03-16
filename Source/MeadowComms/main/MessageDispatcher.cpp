/*
 * MessageDispatcher.cpp
 *
 *  Created on: 15 Feb 2019
 *      Author: mark
 */

#include "MessageDispatcher.hpp"
#include "TransportProviderBase.hpp"
#include "SharedEnums.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                         Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/*
 *      Default constructor.
 */
MessageDispatcher::MessageDispatcher()
{
}

/*
 *      Create a new instance of the message dispatcher with a an associated transport provider.
 *
 *      @param transportProvider
 *          Hardware level transport provider that will deliver and receive the message frames.
 */
MessageDispatcher::MessageDispatcher(TransportProviderBase *transportProvider)
{
    _transportProvider = transportProvider;
}

/*
 *      Default destructor.
 */
MessageDispatcher::~MessageDispatcher()
{
}

/* ---------------------------------------------------------------------------
 *
 *                                  Methods
 *
 * ---------------------------------------------------------------------------
 */

bool MessageDispatcher::QueueMessageForEsp32(Esp32Messaging::Message *request)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOK;
    RequestHandlerBase *provider = nullptr;

    switch ((Esp32Interfaces::Esp32Interfaces) request->Interface)
    {
        case Esp32Interfaces::WiFi:
            if (!_wifiRequestHandler)
            {
                _wifiRequestHandler = new WiFiRequestHandler(this);
            }
            provider = (RequestHandlerBase *) _wifiRequestHandler;
            break;
        case Esp32Interfaces::BlueTooth:
            if (!_bluetoothRequestHandler)
            {
                _bluetoothRequestHandler = new BluetoothRequestHandler(this);
            }
            provider = (RequestHandlerBase *) _bluetoothRequestHandler;
            break;
        case Esp32Interfaces::MeshNetwork:
            if (!_meshNetworkRequestHandler)
            {
                _meshNetworkRequestHandler = new MeshNetworkRequestHandler(this);
            }
            provider = (RequestHandlerBase *) _meshNetworkRequestHandler;
            break;
        case Esp32Interfaces::System:
            if (!_systemRequestHandler)
            {
                _systemRequestHandler = new SystemRequestHandler(this);
            }
            provider = (RequestHandlerBase *) _systemRequestHandler;
            break;
        case Esp32Interfaces::TransportProvider:
        default:
            break;
    }
    if (!provider->QueueEsp32Message(request))
    {
        result = StatusCodes::QueueError;
    }
    return(result);
}

void MessageDispatcher::Setup()
{
    _transportProvider->Setup(this);
}
