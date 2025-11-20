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
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "WiFiRequestHandler.hpp"
#include "WiFiRequestHandlerTestFriend.hpp"
#include "test_common.hpp"
#include "Utils.hpp"

#include "secrets.h"

#define CLASS_OF_TESTS "[WiFi]"

/**
 * @brief Define the DEBUGGING_TESTS macro to enable debug statements.
 */
#define DEBUGGING_TESTS 1

/**
 * @brief Number of scan requests to be made in the multiple scan requests test.
 */
#define NUMBER_OF_SCAN_REQUESTS 500

/**
 *  Extract the AccessPointList object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPointList object.
 *
 *  @returns
 *      Pointer to a AccessPointList object.
 */
static Esp32Messaging::AccessPointList *ExtractAccessPointList(uint8_t *buffer)
{
    // cppcheck-suppress cstyleCast
    Esp32Messaging::AccessPointList *accessPointList = (Esp32Messaging::AccessPointList *) pvPortMalloc(sizeof(Esp32Messaging::AccessPointList));

    accessPointList->NumberOfAccessPoints = Encoders::ExtractUInt32(buffer);
    buffer += 4;
    accessPointList->AccessPointsLength = Encoders::ExtractUInt32(buffer);
    buffer += 4;
    if (accessPointList->AccessPointsLength > 0)
    {
        // cppcheck-suppress cstyleCast
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
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPoint object.
 *
 *  @returns
 *      Pointer to a AccessPoint object.
 */
static Esp32Messaging::AccessPoint *ExtractAccessPoint(uint8_t *buffer)
{
    // cppcheck-suppress cstyleCast
    Esp32Messaging::AccessPoint *accessPoint = (Esp32Messaging::AccessPoint *) pvPortMalloc(sizeof(Esp32Messaging::AccessPoint));

    memcpy((void *) accessPoint->Ssid, (void *) buffer, 33); // cppcheck-suppress cstyleCast
    buffer += 33;
    memcpy((void *) accessPoint->Bssid, (void *) buffer, 6); // cppcheck-suppress cstyleCast
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
 * @brief Test scanning for access points.
 *
 * @note Set number of scans to a number greater than 1 to test for memory leaks.  The number
 *       should be sufficiently high to cause an accumulated issue.
 *
 * @param numberOfScans Number of scans to perform (default to 1).
 */
static void TestScanForAccessPoints(unsigned numberOfScans = 1)
{
    NvsManager::Setup();

    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    WiFiRequestHandler *wifiRequestHandler = new WiFiRequestHandler(messageDispatcher);
    TEST_ASSERT_NOT_NULL(wifiRequestHandler);
    wifiRequestHandler->Setup();

    WiFiRequestHandlerTestFriend *wifiRequestHandlerTestFriend = new WiFiRequestHandlerTestFriend(wifiRequestHandler);

    if (numberOfScans > 1)
    {
        printf("Performing %u scans for access points.\n", numberOfScans);
    }
    else
    {
        printf("Performing a single scan for access points.\n");
    }
    for (unsigned index = 0; index < numberOfScans; index++)
    {
        Message *scanRequestMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, WiFiFunction::GetAccessPoints, StatusCodes::Failure);
        TEST_ASSERT_NOT_NULL(scanRequestMessage);

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
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response));

        TEST_ASSERT_NOT_NULL(accessPointList);
        TEST_ASSERT_GREATER_THAN(0, accessPointList->NumberOfAccessPoints);

        uint8_t *buffer = accessPointList->AccessPoints;
        printf("Scan %u - Number of access points found: %lu\n", (index + 1), accessPointList->NumberOfAccessPoints);
        for (unsigned accessPointNumber = 0; accessPointNumber < accessPointList->NumberOfAccessPoints; accessPointNumber++)
        {
            Esp32Messaging::AccessPoint *accessPoint = ExtractAccessPoint(buffer);
            TEST_ASSERT_NOT_NULL(accessPoint);
            buffer += Encoders::EncodedAccessPointBufferSize(const_cast<Esp32Messaging::AccessPoint *>(accessPoint));
            if (numberOfScans == (index + 1)) // Display the results of the final scan.
            {
                printf("Access Point %u\n", accessPointNumber + 1);
                printf("    Network Name: %s\n", accessPoint->Ssid);
                printf("    MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", accessPoint->Bssid[0], accessPoint->Bssid[1], accessPoint->Bssid[2], accessPoint->Bssid[3], accessPoint->Bssid[4], accessPoint->Bssid[5]);
            }

            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&accessPoint));
        }
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&accessPointList->AccessPoints));
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&accessPointList));
    }

    //
    //  Finally remove the objects we have created.
    //
    delete wifiRequestHandler;
    delete messageDispatcher;
}

/**
 * @brief Test a single scan for access points.
 */
TEST_CASE("Scan for access points", CLASS_OF_TESTS)
{
    TestScanForAccessPoints();
}

/**
 * @brief Perform multiple scans for access points.
 */
TEST_CASE("Scan for access points multiple times (test for memory leak)", CLASS_OF_TESTS)
{
    TestScanForAccessPoints(NUMBER_OF_SCAN_REQUESTS);
}

/**
 * @brief Create a Connect To Access Point Message object using the supplied credentials.
 *
 * @param ssid SSID of the access point.
 * @param password Password for the access point.
 * @return Message* Pointer to the Message with the encoded credentials in the payload.
 */
static Message *CreateConnectToAccessPointMessage(const char *ssid, const char *password)
{
    Esp32Messaging::AccessPointInformation accessPointInformation = {};
    memset(&accessPointInformation, 0, sizeof(Esp32Messaging::AccessPointInformation));
    accessPointInformation.NetworkName = (char *) ssid;  // cppcheck-suppress cstyleCast
    accessPointInformation.Password = (char *) password; // cppcheck-suppress cstyleCast
    uint32_t payloadLength = Encoders::EncodedAccessPointInformationBufferSize(&accessPointInformation);
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    memset(buffer, 0, payloadLength);
    TEST_ASSERT_NOT_NULL(buffer);
    Encoders::EncodeAccessPointInformation(&accessPointInformation, buffer);
    Message *connectToAccessPoint = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, WiFiFunction::ConnectToAccessPoint, StatusCodes::Failure, 0xff, buffer, payloadLength);
    TEST_ASSERT_NOT_NULL(connectToAccessPoint);

    return (connectToAccessPoint);
}

/**
 * @brief Connect to an access point.
 */
TEST_CASE("Connect to access point", CLASS_OF_TESTS)
{
    NvsManager::Setup();

    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    WiFiRequestHandler *wifiRequestHandler = new WiFiRequestHandler(messageDispatcher);
    TEST_ASSERT_NOT_NULL(wifiRequestHandler);
    wifiRequestHandler->Setup();

    Message *connectToAccessPoint = CreateConnectToAccessPointMessage(WIFI_NETWORK, WIFI_PASSWORD);

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
    unsigned timeout = WiFiRequestHandler::GetWiFiTimeout() + 10000;
    bool connectedEvent = false;
    bool connectionResult = false;
    unsigned count = 0;
    WaitForResponse(messageDispatcher, timeout);
    while ((messageDispatcher->OutboundMessageQueueLength() > 0) && !(connectedEvent && connectionResult) && (count < 10))
    {
        count++;
        Message *response = messageDispatcher->GetOutboundMessage();
#if defined(DEBUGGING_TESTS)
        printf("Message: MessageType %d, Function %d, Interface %d, StatusCode %d\n", (int) response->MessageType, (int) response->Function, (int) response->Interface, (int) response->StatusCode);
#endif
        TEST_ASSERT_EQUAL(Esp32Interfaces::WiFi, response->Interface);
        TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
        if ((response->MessageType != MessageTypes::Response) && (response->MessageType != MessageTypes::Event))
        {
            TEST_FAIL_MESSAGE("Unexpected message type in the outbound queue.");
        }
        switch (response->Function)
        {
            case WiFiFunction::ConnectToAccessPoint:
                TEST_ASSERT_EQUAL(response->MessageType, MessageTypes::Response);
                connectionResult = true;
                break;
            case WiFiFunction::NetworkConnectingEvent:
                TEST_ASSERT_EQUAL(response->MessageType, MessageTypes::Event);
                break;
            case WiFiFunction::NetworkConnectedEvent:
                TEST_ASSERT_EQUAL(response->MessageType, MessageTypes::Event);
                connectedEvent = true;
                break;
            default:
                TEST_FAIL_MESSAGE("Unexpected function in the outbound queue.");
                break;
        }
        Message::DeleteMessage(response);
        WaitForResponse(messageDispatcher, timeout);
    }
    printf("Test completeed.\n");
    TEST_ASSERT_TRUE(connectedEvent);
    TEST_ASSERT_TRUE(connectionResult);
    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    //
    //  Finally remove the objects we have created.
    //
    delete wifiRequestHandler;
    delete messageDispatcher;
}

/**
 * @brief Test connecting to an access point that does not exist.
 *
 * @note This is designed to test the automatic reconnect attempts as well as the final failure of the connection request.
 */
TEST_CASE("Connect to non-existant access point", CLASS_OF_TESTS)
{
    NvsManager::Setup();

    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    WiFiRequestHandler *wifiRequestHandler = new WiFiRequestHandler(messageDispatcher);
    TEST_ASSERT_NOT_NULL(wifiRequestHandler);
    wifiRequestHandler->Setup();

    Message *connectToAccessPoint = CreateConnectToAccessPointMessage("BadAccessPoint", "BadPassword");

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
    //  Message: MessageType 4, Function 47, Interface 1, StatusCode 0
    //  Message: MessageType 4, Function 46, Interface 1, StatusCode 0
    //  Message: MessageType 4, Function 37, Interface 1, StatusCode 0
    //  Message: MessageType 16, Function 2, Interface 1, StatusCode 16
    //
    //  The first two messages are examples of the event messages as the connection progresses.  The final
    //  message is the response to the original connection request.
    //
    //  The connection request will take some time so we need to wait for a response to be generated.
    //
    uint32_t timeout = WiFiRequestHandler::GetWiFiTimeout() + 10;
    WaitForResponse(messageDispatcher, timeout);
    bool done = false;
    while ((messageDispatcher->OutboundMessageQueueLength() > 0) && (!done))
    {
        Message *response = messageDispatcher->GetOutboundMessage();
#if defined(DEBUGGING_TESTS)
        printf("Message: MessageType %d, Function %d, Interface %d, StatusCode %d\n", (int) response->MessageType, (int) response->Function, (int) response->Interface, (int) response->StatusCode);
#endif
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
                TEST_ASSERT_EQUAL(response->MessageType, MessageTypes::Response);
                TEST_ASSERT_EQUAL(StatusCodes::CannotConnectToAccessPoint, response->StatusCode);
                done = true;
                break;
            case WiFiFunction::NetworkConnectingEvent:
                TEST_ASSERT_EQUAL(response->MessageType, MessageTypes::Event);
                break;
            case WiFiFunction::NetworkConnectionRetryCountExceededEvent:
                TEST_ASSERT_EQUAL(response->MessageType, MessageTypes::Event);
                break;
            case WiFiFunction::NetworkDisconnectedEvent:
                TEST_ASSERT_EQUAL(response->MessageType, MessageTypes::Event);
                break;
            default:
                TEST_FAIL_MESSAGE("Unexpected function in the outbound queue.");
                break;
        }
        Message::DeleteMessage(response);
        if (!done)
        {
            WaitForResponse(messageDispatcher, timeout);
        }
    }

    //
    //  Finally remove the objects we have created.
    //
    delete wifiRequestHandler;
    delete messageDispatcher;
}
