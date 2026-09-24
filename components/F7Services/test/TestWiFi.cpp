/*
 *  TestWiFi.cpp
 *
 *  Implementation of the test methods for the WiFiRequestHandler class.
 *
 *  These tests drive the request handler the way the STM32 does, by queueing messages for
 *  the dispatcher and reading the responses and events back off the outbound queue, so the
 *  encoding, the dispatch and the handler are all exercised together.
 *
 *  One dispatcher serves the whole run.  WiFiRequestHandler::Setup() creates a WiFi task
 *  and replaces the static event group the handler keeps its state in, so a handler per
 *  test would leave a task behind each time.  A test that needs the radio in a particular
 *  state therefore puts it there itself rather than relying on a new handler to have reset
 *  it.
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
#include "Utils.hpp"

#include "TestAssertions.hpp"
#include "TestEncoders.hpp"
#include "TestHarness.hpp"
#include "UnitTestConfiguration.hpp"
#include "TestWiFiHelpers.hpp"

//
//  [HardwareOnly] marks a test that cannot run under emulation.  These need a radio,
//  which QEMU does not provide.
//
#define CLASS_OF_TESTS "[WiFi][HardwareOnly]"

/**
 * @brief Class of tests that take a long time to execute.
 */
#define SLOW_TESTS "[WiFi][HardwareOnly][Slow]"

/**
 * @brief Name of the SSID for the soft access point used in tests.
 */
#define AP_SSID "MeadowTestAP"

/**
 * @brief Password for the soft access point used in tests.
 */
#define AP_PASSWORD "MeadowTest1234"

/**
 * @brief Static IP address for the AP DHCP server in network byte order (192.168.10.1).
 */
#define AP_STATIC_IP 0x010AA8C0U

/**
 * @brief Static subnet mask for the AP DHCP server in network byte order (255.255.255.0).
 */
#define AP_STATIC_SUBNET 0x00FFFFFFU

/**
 * @brief Static gateway address for the AP DHCP server in network byte order (192.168.10.1).
 */
#define AP_STATIC_GATEWAY 0x010AA8C0U

/**
 * @brief Number of retries to allow a connection that is expected to fail.
 *
 *  The handler waits GetWiFiTimeout() for either an address or an error before giving up
 *  and answering Timeout, and the error is only raised once the retries have been used
 *  up.  Retries are spaced by ProgressiveDelay, whose periods are
 *  {100, 300, 700, 1000, 5000, 10000, 20000} ms with the last repeating, so the default
 *  of 10 retries spends 97 seconds backing off and the 30 second wait always expires
 *  first.  That is why the same request is answered CannotConnectToAccessPoint on a cold
 *  interface and Timeout on a warm one.
 *
 *  Six retries back off for 17.1 seconds, which finishes inside the timeout with room to
 *  spare while still working through five of the seven delay periods.
 */
#define FAILING_CONNECTION_RETRIES 6

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
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

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
        QueueWiFiRequest(dispatcher, WiFiFunction::GetAccessPoints);

        Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::WiFi, WiFiFunction::GetAccessPoints, RadioTimeout());
        TEST_ASSERT_NOT_NULL_MESSAGE(response, "No response to the scan request.");

        ASSERT_RESPONSE(response, Esp32Interfaces::WiFi, WiFiFunction::GetAccessPoints, StatusCodes::CompletedOk);
        ASSERT_HAS_PAYLOAD(response); // Assumes that there will always be an access point available.

        Esp32Messaging::AccessPointList *accessPointList = TestEncoders::ExtractAccessPointList(response->Payload);
        Message::DeleteMessage(response);

        TEST_ASSERT_NOT_NULL(accessPointList);
        TEST_ASSERT_GREATER_THAN(0, accessPointList->NumberOfAccessPoints);

        uint8_t *buffer = accessPointList->AccessPoints;
        printf("Scan %u - Number of access points found: %" PRIu32 "\n", (index + 1), accessPointList->NumberOfAccessPoints);
        for (unsigned accessPointNumber = 0; accessPointNumber < accessPointList->NumberOfAccessPoints; accessPointNumber++)
        {
            Esp32Messaging::AccessPoint *accessPoint = TestEncoders::ExtractAccessPoint(buffer);
            TEST_ASSERT_NOT_NULL(accessPoint);
            buffer += Encoders::EncodedAccessPointBufferSize(accessPoint);
            if (numberOfScans == (index + 1)) // Display the results of the final scan.
            {
                printf("Access Point %u\n", accessPointNumber + 1);
                printf("    Network Name: %s\n", accessPoint->Ssid);
                printf("    MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", accessPoint->Bssid[0], accessPoint->Bssid[1], accessPoint->Bssid[2], accessPoint->Bssid[3], accessPoint->Bssid[4], accessPoint->Bssid[5]);
            }

            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&accessPoint));
        }
        TestEncoders::FreeAccessPointList(&accessPointList);
    }
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
TEST_CASE("Scan for access points multiple times (test for memory leak)", SLOW_TESTS)
{
    TestScanForAccessPoints(UnitTestConfiguration::ScanIterations());
}

/**
 * @brief Connect to an access point.
 *
 *  A connection request generates a number of event messages reporting the progress of the
 *  connection before the response to the request itself is sent.  A typical sequence is:
 *
 *      Message: MessageType 4, Function 47, Interface 1, StatusCode 0
 *      Message: MessageType 4, Function 36, Interface 1, StatusCode 0
 *      Message: MessageType 16, Function 2, Interface 1, StatusCode 0
 *
 *  The first two messages are the events raised as the connection progresses and the final
 *  message is the response to the original connection request.
 *
 * @note The station is disconnected first.  ConnectToAccessPoint returns
 *       WiFiAlreadyStarted without going near the radio once an address is held, so a
 *       connection left up by an earlier test would stop this one testing anything.
 */
TEST_CASE("Connect to access point", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

    EnsureDisconnected(dispatcher);

    printf("Connecting to access point: %s\n", UnitTestConfiguration::WiFiNetwork());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetAutomaticReconnect(true));

    QueueAccessPointRequest(dispatcher, WiFiFunction::ConnectToAccessPoint, UnitTestConfiguration::WiFiNetwork(), UnitTestConfiguration::WiFiPassword());

    ConnectionProgress progress = {};
    CollectConnectionMessages(dispatcher, RadioTimeout(), &progress);

    printf("Test completed.\n");
    TEST_ASSERT_TRUE(progress.ConnectedEvent);
    TEST_ASSERT_TRUE(progress.ResponseReceived);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, progress.ResponseStatusCode);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);
}

/**
 * @brief Test connecting to an access point that does not exist.
 *
 * @note This is designed to test the automatic reconnect attempts as well as the final
 *       failure of the connection request.
 *
 * @note The station is disconnected first.  Run after "Connect to access point" with a
 *       connection still up, this would be answered WiFiAlreadyStarted rather than
 *       CannotConnectToAccessPoint, and would fail.
 */
TEST_CASE("Connect to non-existant access point", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

    EnsureDisconnected(dispatcher);

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetAutomaticReconnect(true));

    //
    //  Let the retries finish inside the handler's own timeout, so the answer does not
    //  depend on whether the interface was already up.  The old value is put back at the
    //  end; it is not held in NVS, so it is only the rest of this run that is affected.
    //
    const uint32_t originalRetryCount = WiFiRequestHandler::GetMaximumRetryCount();
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetMaximumRetryCount(FAILING_CONNECTION_RETRIES));

    QueueAccessPointRequest(dispatcher, WiFiFunction::ConnectToAccessPoint, "BadAccessPoint", "BadPassword");

    ConnectionProgress progress = {};
    CollectConnectionMessages(dispatcher, RadioTimeout(), &progress);

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetMaximumRetryCount(originalRetryCount));

    TEST_ASSERT_TRUE(progress.ResponseReceived);
    TEST_ASSERT_EQUAL(StatusCodes::CannotConnectToAccessPoint, progress.ResponseStatusCode);
    TEST_ASSERT_FALSE(progress.ConnectedEvent);
}

/**
 * @brief Test that ConnectToDefaultAccessPoint is answered when no default access
 *        point is configured, rather than leaking the request (issue #251).
 *
 * @note This does not need a radio, but is kept HardwareOnly with the rest of this
 *       file's connection tests since it shares WiFiRequestHandler state with them.
 *
 * @note The handler also raises NetworkConnectedEvent with this same status, as the
 *       "managed code" notification of a connection outcome; that event carries no
 *       payload and is not what this test is checking, so WaitForResponseTo discards
 *       it along with anything else that is not the response being waited for.
 */
TEST_CASE("Connect to the default access point when none is configured", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

    char *originalAccessPoint = WiFiRequestHandler::GetDefaultAccessPoint();
    char *originalPassword = WiFiRequestHandler::GetPassword();
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetDefaultAccessPoint(NULL));
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetPassword(NULL));

    QueueWiFiRequest(dispatcher, WiFiFunction::ConnectToDefaultAccessPoint);

    Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::WiFi, WiFiFunction::ConnectToDefaultAccessPoint, RadioTimeout());

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetDefaultAccessPoint(originalAccessPoint));
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetPassword(originalPassword));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&originalAccessPoint));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&originalPassword));

    ASSERT_RESPONSE(response, Esp32Interfaces::WiFi, WiFiFunction::ConnectToDefaultAccessPoint, StatusCodes::DefaultAccessPointNotConfigured);
    Message::DeleteMessage(response);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                       Access Point Tests
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Start a soft access point with the given credentials and verify the response.
 *
 * @param ssid SSID for the access point.
 * @param password Password for the access point.
 * @param ipAddress Optional static IP address for the DHCP server (0 for default).
 * @param subnetMask Optional static subnet mask (0 for default).
 * @param gateway Optional static gateway (0 for default).
 */
static void TestStartAccessPoint(const char *ssid, const char *password, uint32_t ipAddress = 0, uint32_t subnetMask = 0, uint32_t gateway = 0)
{
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

    EnsureAccessPointStopped(dispatcher);

    QueueAccessPointRequest(dispatcher, WiFiFunction::StartAccessPoint, ssid, password, ipAddress, subnetMask, gateway);

    const unsigned timeout = RadioTimeout();
    WaitForResponse(dispatcher, timeout);

    bool apStartedEvent = false;
    bool startResponse = false;
    uint32_t reportedIpAddress = 0;

    while ((dispatcher->OutboundMessageQueueLength() > 0) && !(apStartedEvent && startResponse))
    {
        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);

        printf("Message: MessageType %d, Function %d, Interface %d, StatusCode %d\n", static_cast<int>(response->MessageType), static_cast<int>(response->Function), static_cast<int>(response->Interface), static_cast<int>(response->StatusCode));

        TEST_ASSERT_EQUAL(Esp32Interfaces::WiFi, response->Interface);

        switch (response->Function)
        {
            case WiFiFunction::StartAccessPoint:
                ASSERT_RESPONSE(response, Esp32Interfaces::WiFi, WiFiFunction::StartAccessPoint, StatusCodes::CompletedOk);
                startResponse = true;
                break;
            case WiFiFunction::AccessPointStartedEvent:
                {
                    ASSERT_EVENT(response, Esp32Interfaces::WiFi, WiFiFunction::AccessPointStartedEvent, StatusCodes::CompletedOk);
                    TEST_ASSERT_NOT_NULL(response->Payload);
                    Esp32Messaging::AccessPointInformation *info = Encoders::ExtractAccessPointInformation(response->Payload);
                    TEST_ASSERT_NOT_NULL(info);
                    reportedIpAddress = info->IpAddress;
                    Encoders::FreeAccessPointInformation(&info);
                    apStartedEvent = true;
                    break;
                }
            default:
                TEST_FAIL_MESSAGE("Unexpected function in the outbound queue.");
                break;
        }

        Message::DeleteMessage(response);
        if (!(apStartedEvent && startResponse))
        {
            WaitForResponse(dispatcher, timeout);
        }
    }

    TEST_ASSERT_TRUE(apStartedEvent);
    TEST_ASSERT_TRUE(startResponse);
    TEST_ASSERT_NOT_EQUAL(0, reportedIpAddress);
}

/**
 * @brief Start a soft access point with a known SSID and password.
 */
TEST_CASE("Start access point", CLASS_OF_TESTS)
{
    TestStartAccessPoint(AP_SSID, AP_PASSWORD);
}

/**
 * @brief Start a soft access point with a static IP configuration.
 */
TEST_CASE("Start access point with static IP", CLASS_OF_TESTS)
{
    TestStartAccessPoint(AP_SSID, AP_PASSWORD, AP_STATIC_IP, AP_STATIC_SUBNET, AP_STATIC_GATEWAY);
}

/**
 * @brief Start a soft access point then stop it.
 */
TEST_CASE("Stop access point", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

    const unsigned timeout = RadioTimeout();

    //  Start the access point first.
    QueueAccessPointRequest(dispatcher, WiFiFunction::StartAccessPoint, AP_SSID, AP_PASSWORD);
    WaitForResponse(dispatcher, timeout);

    //  Drain the outbound queue from the start.
    DrainOutboundQueue(dispatcher, 1000);

    //  Now stop the access point.
    QueueWiFiRequest(dispatcher, WiFiFunction::StopAccessPoint);
    WaitForResponse(dispatcher, timeout);

    bool stopResponse = false;
    while (dispatcher->OutboundMessageQueueLength() > 0)
    {
        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);

        printf("Message: MessageType %d, Function %d, Interface %d, StatusCode %d\n", static_cast<int>(response->MessageType), static_cast<int>(response->Function), static_cast<int>(response->Interface), static_cast<int>(response->StatusCode));

        TEST_ASSERT_EQUAL(Esp32Interfaces::WiFi, response->Interface);

        switch (response->Function)
        {
            case WiFiFunction::StopAccessPoint:
                ASSERT_RESPONSE(response, Esp32Interfaces::WiFi, WiFiFunction::StopAccessPoint, StatusCodes::CompletedOk);
                stopResponse = true;
                break;
            case WiFiFunction::AccessPointStoppedEvent:
                TEST_ASSERT_EQUAL(MessageTypes::Event, response->MessageType);
                break;
            default:
                TEST_FAIL_MESSAGE("Unexpected function in the outbound queue.");
                break;
        }

        Message::DeleteMessage(response);
        if (!stopResponse)
        {
            WaitForResponse(dispatcher, timeout);
        }
    }

    TEST_ASSERT_TRUE(stopResponse);
}

/**
 * @brief Attempt to start a soft access point with an invalid configuration (missing SSID).
 */
TEST_CASE("Start access point - invalid configuration", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

    //  Create a message with an empty SSID to trigger validation failure.
    QueueAccessPointRequest(dispatcher, WiFiFunction::StartAccessPoint, "", AP_PASSWORD);

    Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::WiFi, WiFiFunction::StartAccessPoint, RadioTimeout());
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "No response to the start access point request.");

    ASSERT_RESPONSE(response, Esp32Interfaces::WiFi, WiFiFunction::StartAccessPoint, StatusCodes::CannotStartAccessPoint);

    Message::DeleteMessage(response);
}

/**
 * @brief Attempt to stop the soft access point when it is not running.
 *
 * @note The access point is stopped first so that the test states the condition it is
 *       testing rather than depending on which access point test ran before it.
 */
TEST_CASE("Stop access point when not running", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupWiFiDispatcher();

    EnsureAccessPointStopped(dispatcher);

    QueueWiFiRequest(dispatcher, WiFiFunction::StopAccessPoint);

    Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::WiFi, WiFiFunction::StopAccessPoint, RadioTimeout());
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "No response to the stop access point request.");

    ASSERT_RESPONSE(response, Esp32Interfaces::WiFi, WiFiFunction::StopAccessPoint, StatusCodes::AccessPointNotStarted);

    Message::DeleteMessage(response);
}
