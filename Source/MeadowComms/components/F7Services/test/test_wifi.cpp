/*
 *  test_wifi.cpp
 *
 *  Implementation of the test methods for the WiFiRequestHandler class.
 */

#include "sdkconfig.h"

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <string.h>
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "MockTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "WiFiRequestHandler.hpp"
#include "WiFiRequestHandlerTestFriend.hpp"

#include "secrets.h"

/**
 *  Extract the AccessPointList object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPointList object.
 *
 *  @returns
 *      Pointer to a AccessPointList object.
 */

static Esp32Messaging::AccessPointList *ExtractAccessPointList(uint8_t *buffer)
{
    Esp32Messaging::AccessPointList *accessPointList = (Esp32Messaging::AccessPointList *) pvPortMalloc(sizeof(Esp32Messaging::AccessPointList));

    accessPointList->NumberOfAccessPoints = Encoders::ExtractUInt32(buffer);
    buffer += 4;
    accessPointList->AccessPointsLength = Encoders::ExtractUInt32(buffer);
    buffer += 4;
    if (accessPointList->AccessPointsLength > 0)
    {
        accessPointList->AccessPoints = (uint8_t *) pvPortMalloc(accessPointList->AccessPointsLength);
        memcpy(accessPointList->AccessPoints, buffer, accessPointList->AccessPointsLength);
        buffer += accessPointList->AccessPointsLength;
    }
    else
    {
        accessPointList->AccessPoints = nullptr;
    }
    return (accessPointList);
}

/**
 *  Extract the AccessPoint object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPoint object.
 *
 *  @returns
 *      Pointer to a AccessPoint object.
 */
static Esp32Messaging::AccessPoint *ExtractAccessPoint(uint8_t *buffer)
{
    Esp32Messaging::AccessPoint *accessPoint = (Esp32Messaging::AccessPoint *) pvPortMalloc(sizeof(Esp32Messaging::AccessPoint));

    memcpy((void *) accessPoint->Ssid, (void *) buffer, 33);
    buffer += 33;
    memcpy((void *) accessPoint->Bssid, (void *) buffer, 6);
    buffer += 6;
    accessPoint->PrimaryChannel = *buffer;
    buffer += 1;
    accessPoint->SecondaryChannel = *buffer;
    buffer += 1;
    accessPoint->Rssi = (int8_t) *buffer;
    buffer += 1;
    accessPoint->AuthenticationMode = *buffer;
    buffer += 1;
    accessPoint->Protocols = Encoders::ExtractUInt32(buffer);
    return (accessPoint);
}

/**
 * @brief Wait for a response to be received. or for a timeout.
 * 
 * @note This method allows for a request to take a time to respond.
 * 
 * @param seconds Number of seconds to wait.
 */
static void WaitForResponse(MockMessageDispatcher *messageDispatcher, uint32_t seconds)
{
    int count = 0;
    while ((messageDispatcher->OutboundMessageQueueLength() == 0) && (count < seconds))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        count++;
    }
}

/**
 * @brief Test a single scan for access points.
 */
TEST_CASE("Scan for access points", "[WiFi]")
{
    NvsManager::Setup();

    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    WiFiRequestHandler *wifiRequestHandler = new WiFiRequestHandler(messageDispatcher);
    TEST_ASSERT_NOT_NULL(wifiRequestHandler);
    wifiRequestHandler->Setup();

    //
    //  Create a message to format the file system and call the format method.
    //
    Message *scanRequestMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, WiFiFunction::GetAccessPoints, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(scanRequestMessage);

    WiFiRequestHandlerTestFriend *wifiRequestHandlerTestFriend = new WiFiRequestHandlerTestFriend(wifiRequestHandler);
    wifiRequestHandlerTestFriend->ScanForAccessPoints(scanRequestMessage);

    WaitForResponse(messageDispatcher, 20);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::WiFi, response->Interface);
    TEST_ASSERT_EQUAL(WiFiFunction::GetAccessPoints, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength); // Assumes that there will always be an access point available.
    TEST_ASSERT_NOT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());
    //
    //  The ScanForAccessPoints method will have reused the original request message for the response.
    //  We do not need to release the original request as it is now in the response variable and will
    //  be released when we release the response.
    //
    Esp32Messaging::AccessPointList *accessPointList = ExtractAccessPointList(response->Payload);
    response->DeletePayload();
    vPortFree(response);

    TEST_ASSERT_NOT_NULL(accessPointList);
    TEST_ASSERT_GREATER_THAN(0, accessPointList->NumberOfAccessPoints);

    uint8_t *buffer = accessPointList->AccessPoints;
    printf("Number of access points found: %lu\n", accessPointList->NumberOfAccessPoints);
    for (uint32_t i = 0; i < accessPointList->NumberOfAccessPoints; i++)
    {
        const Esp32Messaging::AccessPoint *accessPoint = ExtractAccessPoint(buffer);
        TEST_ASSERT_NOT_NULL(accessPoint);
        buffer += Encoders::EncodedAccessPointBufferSize(const_cast<Esp32Messaging::AccessPoint *>(accessPoint));
        printf("Access Point %lu\n", i + 1);
        printf("    Network Name: %s\n", accessPoint->Ssid);
        printf("    MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", accessPoint->Bssid[0], accessPoint->Bssid[1], accessPoint->Bssid[2], accessPoint->Bssid[3], accessPoint->Bssid[4], accessPoint->Bssid[5]);
        vPortFree((void *) accessPoint);
    }
    vPortFree(accessPointList->AccessPoints);
    vPortFree(accessPointList);

    //
    //  Finally remove the objects we have created.
    //
    delete wifiRequestHandler;
    delete messageDispatcher;
}

/**
 * @brief Connect to an access point.
 */
TEST_CASE("Connect to access point", "[WiFI]")
{
    NvsManager::Setup();

    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    WiFiRequestHandler *wifiRequestHandler = new WiFiRequestHandler(messageDispatcher);
    TEST_ASSERT_NOT_NULL(wifiRequestHandler);
    wifiRequestHandler->Setup();

    //
    //  Create a message to format the file system and call the format method.
    //
    Esp32Messaging::AccessPointInformation accessPointInformation = {};
    memset(&accessPointInformation, 0, sizeof(Esp32Messaging::AccessPointInformation));
    accessPointInformation.NetworkName = (char *) WIFI_NETWORK;
    accessPointInformation.Password = (char *) WIFI_PASSWORD;
    uint32_t payloadLength = Encoders::EncodedAccessPointInformationBufferSize(&accessPointInformation);
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    memset(buffer, 0, payloadLength);
    TEST_ASSERT_NOT_NULL(buffer);
    Encoders::EncodeAccessPointInformation(&accessPointInformation, buffer);
    Message *connectToAccessPoint = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, WiFiFunction::ConnectToAccessPoint, StatusCodes::Failure, 0xff, buffer, payloadLength);
    TEST_ASSERT_NOT_NULL(connectToAccessPoint);

    WiFiRequestHandlerTestFriend *wifiRequestHandlerTestFriend = new WiFiRequestHandlerTestFriend(wifiRequestHandler);
    StatusCodes::StatusCodes result = wifiRequestHandlerTestFriend->SetAutomaticReconnect(true);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, result);
    wifiRequestHandlerTestFriend->ConnectToAccessPoint(connectToAccessPoint);
    //
    //  We need to loop through the messages in the outbound queue and check for a successful connection.
    //  There will be a number of messages generated before the connection is actually made.  These will
    //  be the event messages informing the STM32 of the current state of the connection.  A typical sequence
    //  of messages is:
    //
    //  Message: MessageType 4, Function 47, Interface 1, StatusCode 0
    //  Message: MessageType 4, Function 36, Interface 1, StatusCode 0
    //  Message: MessageType 16, Function 2, Interface 1, StatusCode 0
    //
    //  The first two messages are examples of the event messages as the connection progresses.  The final
    //  message is the response to the original connection request.
    //
    //  The connection request will take some time so we need to wait for a response to be generated.
    //
    uint32_t timeout = WiFiRequestHandler::GetWiFiTimeout() + 10;
    bool connected = false;
    WaitForResponse(messageDispatcher, timeout);
    while (messageDispatcher->OutboundMessageQueueLength() > 0)
    {
        Message *response = messageDispatcher->GetOutboundMessage();
        printf("Message: MessageType %d, Function %d, Interface %d, StatusCode %d\n", (int) response->MessageType, (int) response->Function, (int) response->Interface, (int)response->StatusCode);
        TEST_ASSERT_EQUAL(Esp32Interfaces::WiFi, response->Interface);
        TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
        if ((response->MessageType != MessageTypes::Response) && (response->MessageType != MessageTypes::Event))
        {
            TEST_FAIL_MESSAGE("Unexpected message type in the outbound queue.");
        }
        if ((response->Function != WiFiFunction::ConnectToAccessPoint) && (response->Function != WiFiFunction::NetworkConnectingEvent) && (response->Function != WiFiFunction::NetworkConnectedEvent))
        {
            TEST_FAIL_MESSAGE("Unexpected function in the outbound queue.");
        }
        if ((response->MessageType == MessageTypes::Response) && (response->StatusCode == StatusCodes::CompletedOk) && (response->Function != WiFiFunction::NetworkConnectedEvent))
        {
            connected = true;
        }
        Message::DeleteMessage(response);
        WaitForResponse(messageDispatcher, timeout);
    }
    TEST_ASSERT_TRUE(connected);
    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());
    
    //
    //  Finally remove the objects we have created.
    //
    delete wifiRequestHandler;
    delete messageDispatcher;
}
