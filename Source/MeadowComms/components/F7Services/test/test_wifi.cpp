/*
 *  test_file_system.cpp
 *
 *  Implementation of the test methods for the file system.
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
    return(accessPointList);
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
    return(accessPoint);
}

class WiFiRequestHandlerTestFriend
{
    private:
        WiFiRequestHandler *wifiRequestHandler;

    public:
        WiFiRequestHandlerTestFriend(WiFiRequestHandler *wifiRequestHandler)
        {
            wifiRequestHandler = wifiRequestHandler;
        }

        void Setup()
        {
            wifiRequestHandler->Setup();
        }

        void ScanForAccessPoints(Message *message)
        {
            wifiRequestHandler->ScanForAccessPoints(message);
        }
};

/**
 * @brief Test a single scan for access points.
 *
 *  This test will test formatting the LittleFS file system as follows:
 *      - Format the file system and verify that a single success response is received from
 *        the FileSystem class.
 *      - Write a single file to the newly formatted file system using c stdio methods and
 *        verify that there is exactly one file on the file system.
 *      - Reformat the file system and verify that no files can be found.
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

    //
    //  Give the Wifi system a little time to start the interface and perform the scan.
    //  20 seconds should be enough but allow for the fact that it might actually
    //  complete sooner.
    //
    int count = 0;
    while ((messageDispatcher->OutboundMessageQueueLength() == 0) && (count < 20))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        count++;
    }

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
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);           // Assumes that there will always be an access point available.
    TEST_ASSERT_NOT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());
    //
    //  The ScanForAccessPoints method will have reused the original request message for the response.
    //  We do not need to release the original request as it is now in the response variable and will
    //  be released when we release the response.
    //
    Esp32Messaging::AccessPointList *accessPointList = ExtractAccessPointList(response->Payload);
    vPortFree(response->Payload);
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
