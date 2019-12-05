/*
 *  MessageDispatcher.cpp
 *
 *  Implement the methods that will allow the transmission and reception
 *  of messages between the ESP32 and the STM32.
 */

#include "MessageDispatcher.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  Default constructor.
 */
MessageDispatcher::MessageDispatcher() : _messageIdMutex(xSemaphoreCreateMutex())
{
}

/**
 *  Create a new instance of the message dispatcher with an associated transport provider.
 *
 *  @param transportProvider
 *      Hardware level transport provider that will deliver and receive the messages.
 */
MessageDispatcher::MessageDispatcher(TransportProviderBase *transportProvider) : MessageDispatcher()
{
    _transportProvider = transportProvider;
    _transportProvider->Setup(this);
}

/**
 *  Default destructor.
 */
MessageDispatcher::~MessageDispatcher()
{
}

/* ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  Add the specified message (request) to the appropriate interface
 *  message queue.
 *
 *  @param request
 *      Request from the STM32 to the ESP32.
 *
 *  @return
 *      True if the request could be queue, false otherwise.
 */
bool MessageDispatcher::QueueMessageForEsp32(Message *request)
{
    RequestHandlerBase *provider = nullptr;

    switch ((Esp32Interfaces::Esp32Interfaces) request->Interface)
    {
        case Esp32Interfaces::WiFi:
            if (!_wifiRequestHandler)
            {
                _wifiRequestHandler = new WiFiRequestHandler(this);
                _wifiRequestHandler->Setup();
            }
            provider = dynamic_cast<RequestHandlerBase *>(_wifiRequestHandler);
            break;
        case Esp32Interfaces::BlueTooth:
            if (!_bluetoothRequestHandler)
            {
                _bluetoothRequestHandler = new BluetoothRequestHandler(this);
                _bluetoothRequestHandler->Setup();
            }
            provider = dynamic_cast<RequestHandlerBase *>(_bluetoothRequestHandler);
            break;
        case Esp32Interfaces::MeshNetwork:
            if (!_meshNetworkRequestHandler)
            {
                _meshNetworkRequestHandler = new MeshNetworkRequestHandler(this);
                _meshNetworkRequestHandler->Setup();
            }
            provider = dynamic_cast<RequestHandlerBase *>(_meshNetworkRequestHandler);
            break;
        case Esp32Interfaces::System:
            if (!_systemRequestHandler)
            {
                _systemRequestHandler = new SystemRequestHandler(this);
                _systemRequestHandler->Setup();
            }
            provider = dynamic_cast<RequestHandlerBase *>(_systemRequestHandler);
            break;
        default:
            ESP_LOGI(__func__, "Unknown interface %d, message will not be processed.", request->Interface);
            break;
    }
    bool result = false;
    if (provider)
    {
        result = provider->QueueEsp32Message(request);
    }
    return(result);
}

/**
 *  Add the response from the system to the hardware transport provider
 *  queue of messages waiting to be sent to the STM32.
 *
 *  @param response
 *      Response from the ESP32 to a request from the STM32.
 *
 *  @return
 *      True if the response was queued correctly, false otherwise.
 */
bool MessageDispatcher::QueueMessageForStm32(Message *response)
{
    return(_transportProvider->QueueMessageForStm32(response));
}

/**
 *  Get the next message ID that can be used by the ESP32 system to send a
 *  message to the STM32.
 *
 *  This method will increment the last used message ID, apply the message
 *  ID mask and then return the Message ID.
 *
 *  @returns
 *      Next message ID that can be used.
 */
uint32_t MessageDispatcher::GetNextMessageID()
{
    xSemaphoreTake(_messageIdMutex, portMAX_DELAY);
    _lastMessageId++;
    uint32_t messageId = _lastMessageId;
    xSemaphoreGive(_messageIdMutex);
    messageId |= ESP32_MESSAGE_ID_MASK;
    return(messageId);
}
