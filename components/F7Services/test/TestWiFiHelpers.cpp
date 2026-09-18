/**
 * @file TestWiFiHelpers.cpp
 * @author Mark Stevens
 * @brief WiFi setup and connection methods shared by the WiFi and POSIX tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "freertos/FreeRTOS.h"

#include "Encoders.hpp"
#include "NvsManager.hpp"
#include "Utils.hpp"

#include "TestHarness.hpp"
#include "TestWiFiHelpers.hpp"

#include "secrets.h"

/**
 * @brief Number of times a connection to the access point should be attempted.
 */
static const int CONNECTION_ATTEMPTS = 3;

/**
 * @brief Largest number of messages to read while waiting for a connection response.
 */
static const unsigned MAXIMUM_CONNECTION_MESSAGES = 10;

/**
 * @brief Milliseconds allowed for the access point to stop.
 */
static const unsigned STOP_ACCESS_POINT_TIMEOUT = 5000;

/**
 * @brief Whether the access point has already been joined during this run.
 */
static bool _connectedToAccessPoint = false;

/**
 * @brief Prepare the message dispatcher the WiFi tests drive the radio through.
 *
 * @return Pointer to the dispatcher the tests should use.
 */
MockMessageDispatcher *SetupWiFiDispatcher()
{
    NvsManager::Setup();

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    return (dispatcher);
}

/**
 * @brief Milliseconds to allow for a request that has to wait for the radio.
 *
 * @return Number of milliseconds to wait.
 */
unsigned RadioTimeout()
{
    return (WiFiRequestHandler::GetWiFiTimeout() + 10000);
}

/**
 * @brief Create a message carrying an encoded AccessPointInformation object.
 *
 * @param function Function the message should request.
 * @param ssid SSID of the access point.
 * @param password Password for the access point.
 * @param ipAddress Static IP address for the access point, 0 to use the default.
 * @param subnetMask Subnet mask for the access point, 0 to use the default.
 * @param gateway Gateway for the access point, 0 to use the default.
 *
 * @return Pointer to a message holding the encoded credentials in its payload.
 */
Message *CreateAccessPointMessage(const uint32_t function, const char *ssid, const char *password, const uint32_t ipAddress, const uint32_t subnetMask, const uint32_t gateway)
{
    Esp32Messaging::AccessPointInformation accessPointInformation = {};
    memset(&accessPointInformation, 0, sizeof(Esp32Messaging::AccessPointInformation));
    accessPointInformation.NetworkName = const_cast<char *>(ssid);
    accessPointInformation.Password = const_cast<char *>(password);
    accessPointInformation.IpAddress = ipAddress;
    accessPointInformation.SubnetMask = subnetMask;
    accessPointInformation.Gateway = gateway;

    uint32_t payloadLength = Encoders::EncodedAccessPointInformationBufferSize(&accessPointInformation);
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, 0, payloadLength);
    Encoders::EncodeAccessPointInformation(&accessPointInformation, buffer);

    Message *message = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, function, StatusCodes::Failure, 0xff, buffer, payloadLength);
    TEST_ASSERT_NOT_NULL(message);

    return (message);
}

/**
 * @brief Queue a request carrying a network name and password.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param function Function the message should request.
 * @param ssid SSID of the access point.
 * @param password Password for the access point.
 * @param ipAddress Static IP address for the access point, 0 to use the default.
 * @param subnetMask Subnet mask for the access point, 0 to use the default.
 * @param gateway Gateway for the access point, 0 to use the default.
 */
void QueueAccessPointRequest(MockMessageDispatcher *dispatcher, const uint32_t function, const char *ssid, const char *password, const uint32_t ipAddress, const uint32_t subnetMask, const uint32_t gateway)
{
    Message *request = CreateAccessPointMessage(function, ssid, password, ipAddress, subnetMask, gateway);

    dispatcher->QueueMessageForEsp32(request);
}

/**
 * @brief Queue a request that carries no payload.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param function Function the message should request.
 */
void QueueWiFiRequest(MockMessageDispatcher *dispatcher, const uint32_t function)
{
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, function, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(request);

    dispatcher->QueueMessageForEsp32(request);
}

/**
 * @brief Read the messages a connection request generates until its response arrives.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 * @param timeout Milliseconds to wait for each message.
 * @param progress Filled in with the outcome of the connection attempt.
 */
void CollectConnectionMessages(MockMessageDispatcher *dispatcher, const unsigned timeout, ConnectionProgress *progress)
{
    progress->ConnectedEvent = false;
    progress->ResponseReceived = false;
    progress->ResponseStatusCode = StatusCodes::Failure;

    unsigned count = 0;
    WaitForResponse(dispatcher, timeout);
    while ((dispatcher->OutboundMessageQueueLength() > 0) && !progress->ResponseReceived && (count < MAXIMUM_CONNECTION_MESSAGES))
    {
        count++;

        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);

        printf("Message: MessageType %d, Function %d, Interface %d, StatusCode %d\n", static_cast<int>(response->MessageType), static_cast<int>(response->Function), static_cast<int>(response->Interface), static_cast<int>(response->StatusCode));

        TEST_ASSERT_EQUAL(Esp32Interfaces::WiFi, response->Interface);
        if ((response->MessageType != MessageTypes::Response) && (response->MessageType != MessageTypes::Event))
        {
            TEST_FAIL_MESSAGE("Unexpected message type in the outbound queue.");
        }
        if (response->MessageType == MessageTypes::Event)
        {
            TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
        }

        switch (response->Function)
        {
            case WiFiFunction::ConnectToAccessPoint:
                TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
                progress->ResponseStatusCode = response->StatusCode;
                progress->ResponseReceived = true;
                break;
            case WiFiFunction::NetworkConnectedEvent:
                TEST_ASSERT_EQUAL(MessageTypes::Event, response->MessageType);
                progress->ConnectedEvent = true;
                break;
            case WiFiFunction::NetworkConnectingEvent:
            case WiFiFunction::NetworkConnectionRetryCountExceededEvent:
            case WiFiFunction::NetworkDisconnectedEvent:
                TEST_ASSERT_EQUAL(MessageTypes::Event, response->MessageType);
                break;
            default:
                TEST_FAIL_MESSAGE("Unexpected function in the outbound queue.");
                break;
        }

        Message::DeleteMessage(response);
        if (!progress->ResponseReceived)
        {
            WaitForResponse(dispatcher, timeout);
        }
    }
}

/**
 * @brief Stop the soft access point, whether or not it is running.
 *
 * @param dispatcher Dispatcher to queue the request on.
 */
void EnsureAccessPointStopped(MockMessageDispatcher *dispatcher)
{
    QueueWiFiRequest(dispatcher, WiFiFunction::StopAccessPoint);

    Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::WiFi, WiFiFunction::StopAccessPoint, STOP_ACCESS_POINT_TIMEOUT);
    if (response != nullptr)
    {
        Message::DeleteMessage(response);
    }

    //
    //  Stopping the access point raises an event of its own, which would otherwise be
    //  waiting in the queue when the test makes its first real request.
    //
    DrainOutboundQueue(dispatcher, 0);
}

/**
 * @brief Connect to the access point named in secrets.h.
 *
 * @param dispatcher Dispatcher to queue the request on.
 *
 * @return True if the connection was made.
 */
bool ConnectToTestAccessPoint(MockMessageDispatcher *dispatcher)
{
    if (_connectedToAccessPoint)
    {
        return (true);
    }

    const unsigned timeout = RadioTimeout();

    for (int attempt = 1; (attempt <= CONNECTION_ATTEMPTS) && !_connectedToAccessPoint; attempt++)
    {
        printf("Connecting to access point %s, attempt %d.\n", WIFI_NETWORK, attempt);

        QueueAccessPointRequest(dispatcher, WiFiFunction::ConnectToAccessPoint, WIFI_NETWORK, WIFI_PASSWORD);

        Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::WiFi, WiFiFunction::ConnectToAccessPoint, timeout);
        if (response != nullptr)
        {
            _connectedToAccessPoint = (response->StatusCode == StatusCodes::CompletedOk);
            Message::DeleteMessage(response);
        }
    }

    return (_connectedToAccessPoint);
}
