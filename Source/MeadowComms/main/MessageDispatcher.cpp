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

/*
 * ---------------------------------------------------------------------------
 *
 *                           Getters and Setters
 *
 * ---------------------------------------------------------------------------
 */

BluetoothRequestHandler* MessageDispatcher::GetBluetoothRequestHandler()
{
    return _bluetoothRequestHandler;
}

void MessageDispatcher::SetBluetoothRequestHandler(BluetoothRequestHandler* bluetoothRequestHandler)
{
    _bluetoothRequestHandler = bluetoothRequestHandler;
}

MeshNetworkRequestHandler* MessageDispatcher::GetMeshNetworkRequestHandler()
{
    return _meshNetworkRequestHandler;
}

void MessageDispatcher::SetMeshNetworkRequestHandler(MeshNetworkRequestHandler* meshNetworkRequestHandler)
{
    _meshNetworkRequestHandler = meshNetworkRequestHandler;
}

SystemRequestHandler* MessageDispatcher::GetSystemRequestHandler()
{
    return _systemRequestHandler;
}

void MessageDispatcher::SetSystemRequestHandler(SystemRequestHandler* systemRequestHandler)
{
    _systemRequestHandler = systemRequestHandler;
}

WiFiRequestHandler* MessageDispatcher::GetWifiRequestHandler()
{
    return _wifiRequestHandler;
}

void MessageDispatcher::SetWifiRequestHandler(WiFiRequestHandler* wifiRequestHandler)
{
    _wifiRequestHandler = wifiRequestHandler;
}

/* ---------------------------------------------------------------------------
 *
 *                                  Methods
 *
 * ---------------------------------------------------------------------------
 */

StatusCodes::StatusCodes MessageDispatcher::QueueMessage(ApplicationRequest *request)
{
    QueueHandle_t queueHandle = NULL;

    switch (request->_interface)
    {
        case INTERFACE_WIFI:
            queueHandle = _wifiRequestHandler->GetQueueHandle();
            break;
        case INTERFACE_BLUETOOTH:
            queueHandle = _bluetoothRequestHandler->GetQueueHandle();
            break;
        case INTERFACE_MESH_NETWORK:
            queueHandle = _meshNetworkRequestHandler->GetQueueHandle();
            break;
        case INTERFACE_SYSTEM:
            queueHandle = _systemRequestHandler->GetQueueHandle();
            break;
    }
    if (xQueueSendToBack(queueHandle, request, 0) != pdPASS)
    {
        //
        //  TODO: Need to work out what is best to do when queueing messages fails.
        //
        if (request->_payload != NULL)
        {
            vPortFree(request->_payload);
        }
        return (StatusCodes::QueueError);
    }
    return (StatusCodes::CompletedOK);
}

/*
 *      Reset the message header and delete the message body storage.
 */
void MessageDispatcher::ResetIncomingMessage(bool releasePayloadMemory)
{
    _messageHeader._interface = 0;
    _messageHeader._function = 0;
    _messageHeader._messageID = 0;
    if ((_messageHeader._payload != NULL) && releasePayloadMemory)
    {
        vPortFree(_messageHeader._payload);
    }
    _messageHeader._payload = NULL;
    _messageHeader._payloadLength = 0;
    _bytesRemaining  = 0;
    _nextPayloadLocation = NULL;
}

/*
 *      Process an incoming application header packet.
 *
 *      This method will extract the message information (ID, interface etc) from the incoming
 *      message packet.
 *
 *      @param packet
 *          Pointer to the header packet to be processed.
 *
 *      @param packetLength
 *          Amount of data in the packet.
 */
void MessageDispatcher::ProcessHeader(uint8_t *packet, uint8_t packetLength)
{
    ResetIncomingMessage(true);
    _messageHeader._interface = packet[0];
    _messageHeader._function = Encoders::ExtractUInt32(packet + APP_REQUEST_FUNCTION);
    _messageHeader._messageID = Encoders::ExtractUInt32(packet + APP_REQUEST_MESSAGE_ID);
    _bytesRemaining = Encoders::ExtractUInt32(packet + APP_REQUEST_PAYLOAD_LENGTH);
    _messageHeader._payloadLength = _bytesRemaining;
    if (_messageHeader._payloadLength > 0)
    {
        _messageHeader._payload = (uint8_t *) pvPortMalloc(_messageHeader._payloadLength);
    }
    else
    {
        _messageHeader._payload = NULL;
        //
        //  Messages with no payload should be queued immediately.
        //
        QueueMessage();
    }
    _nextPayloadLocation = _messageHeader._payload;
}

/*
 *      Process an application data packet.
 *
 *      The application frames will be reassembled into a larger message (if required).  The message
 *      will be added to the appropriate message queue for processing once a full message has been
 *      received.
 *
 *      @param packet
 *          Pointer to the header packet to be processed.
 *
 *      @param packetLength
 *          Amount of data in the packet.
 */
StatusCodes::StatusCodes MessageDispatcher::ProcessData(uint8_t *packet, uint8_t packetLength)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOK;

    if ((_nextPayloadLocation != NULL) && (_bytesRemaining > 0) && (packetLength <= _bytesRemaining))
    {
        //
        //  TODO: Change this to use the sequence number for this message to determine where the data should go.
        //
        memcpy(_nextPayloadLocation, packet, packetLength);
        _nextPayloadLocation += packetLength;
        _bytesRemaining -= packetLength;
        if (_bytesRemaining == 0)
        {
            QueueMessage();
            //
            //  It is important NOT to release the memory containing the payload
            //  as the pointer to the payload has been copied into the queue for
            //  the appropriate interface.
            //
            //  It is the responsibility of the queue handler to release the payload
            //  memory when the handler has finished processing the message.
            //  Releasing the memory here will corrupt the heap.
            //
            ResetIncomingMessage(false);
        }
        result = StatusCodes::CompletedOK;
    }
    else
    {
        StatusCodes::StatusCodes result = StatusCodes::UnexpectedData;
    }
    return(result);
}

StatusCodes::StatusCodes MessageDispatcher::ProcessApplicationDataFrame(uint8_t frameType, uint8_t *packet, uint8_t packetLength)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOK;
    //
    //  Application data packets are either header or data packets.
    //
    if (frameType & DATA_HEADER)
    {
        result = ProcessHeader(packet, packetLength);
    }
    else
    {
        result = ProcessData(packet, packetLength);
    }
    return(result);
}
