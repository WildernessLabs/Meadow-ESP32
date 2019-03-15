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

StatusCodes::StatusCodes MessageDispatcher::QueueMessage(Esp32Messaging::Message *request)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOK;
    TransportProviderBase *provider = NULL;

    switch ((Esp32Interfaces::Esp32Interfaces) request->Interface)
    {
        case Esp32Interfaces::WiFi:
            provider = (TransportProviderBase *) _wifiRequestHandler;
            break;
        case Esp32Interfaces::BlueTooth:
            provider = (TransportProviderBase *) _bluetoothRequestHandler;
            break;
        case Esp32Interfaces::MeshNetwork:
            provider = (TransportProviderBase *) _meshNetworkRequestHandler;
            break;
        case Esp32Interfaces::System:
            provider = (TransportProviderBase *) _systemRequestHandler;
            break;
        case Esp32Interfaces::TransportProvider:
        default:
            break;
    }
    if (!provider->QueueMessageForStm32(request))
    {
        result = StatusCodes::QueueError;
    }
    return(result);
}

void MessageDispatcher::Setup()
{
    _transportProvider->Setup();
}
