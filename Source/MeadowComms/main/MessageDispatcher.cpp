/*
 *      MessageDispatcher.cpp
 *
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

/*
 *      Add the specified message (request) to the appropriate interface
 *      message queue.
 *
 *      @param request
 *          Request from the STM32 to the ESP32.
 *
 *      @return
 *          True if the request could be queue, false otherwise.
 */
bool MessageDispatcher::QueueMessageForEsp32(Esp32Messaging::Message *request)
{
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
    return(provider->QueueEsp32Message(request));
}

/*
 *      Add the response from the system to the hardware transport provider
 *      queue of messages waiting to be sent to the STM32.
 *
 *      @param response
 *          Response from the ESP32 to a request from the STM32.
 *
 *      @return
 *          True if the response was queued correctly, false otherwise.
 */
bool MessageDispatcher::QueueMessageForStm32(Esp32Messaging::Message *response)
{
    return(_transportProvider->QueueMessageForStm32(response));
}

/*
 *      Setup the Message Dispatcher object.
 */
void MessageDispatcher::Setup()
{
    _transportProvider->Setup(this);
}
