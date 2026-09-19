/*
 *  TestBluetooth.cpp
 *
 *  Implementation of the test methods for the BluetoothRequestHandler class.
 */

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
#include "Logging.hpp"

#include "TestAssertions.hpp"
#include "TestBluetoothHelpers.hpp"
#include "TestHarness.hpp"
#include "UnitTestConfiguration.hpp"
#include "TestBluetooth.hpp"

//
//  [HardwareOnly] marks a test that cannot run under emulation.  These need a radio,
//  which QEMU does not provide.
//
#define CLASS_OF_TESTS "[Bluetooth][HardwareOnly]"

/**
 * @brief Milliseconds allowed for the user to connect a client.
 */
#define CLIENT_CONNECTION_TIMEOUT 60000

/**
 * @brief Milliseconds allowed for the user to subscribe to notifications.
 */
#define CLIENT_SUBSCRIPTION_TIME 10000

/**
 * @brief Handle of the characteristic written by the notification tests.
 */
#define NOTIFICATION_CHARACTERISTIC_HANDLE 0x2a

/**
 * @brief Value written to a characteristic by the notification tests.
 */
#define NOTIFICATION_VALUE 0xa0a0a0a0

/**
 * @brief Number of bytes of handle data the MeadowF7 service returns.
 *
 *  The handles are all uint16_t values.  The first value is the handle for the service,
 *  and each characteristic contributes the handle for its declaration followed by the
 *  handle for its value.
 */
#define MEADOW_F7_HANDLES_PAYLOAD_LENGTH 24

/**
 * @brief Names and UUIDs of the characteristics in the MeadowF7 service.
 */
const char *ON_OFF_NAME = "OnOff";
const char *ON_OFF_UUID = "016e99d6-8a61-11eb-8dcd-0242ac1300ff";

const char *BOOLEAN_FIELD_NAME = "BooleanField";
const char *BOOLEAN_FIELD_UUID = "017e99d6-8a61-11eb-8dcd-0242ac1300aa";

const char *NUMBER_FIELD_NAME = "NumberField";
const char *NUMBER_FIELD_UUID = "018e99d6-8a61-11eb-8dcd-0242ac1300bb";

const char *TEXT_FIELD_NAME = "TextField";
const char *TEXT_FIELD_UUID = "019e99d6-8a61-11eb-8dcd-0242ac1300cc";

/**
 * @brief This is really a dummy method and is used to trap the end of a test method
 *        so that gdb can halt heap tracing to host.
 */
void EndOfTest()
{
    __asm__("nop");
}

/**
 * @brief Wait for a client to connect to the service.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 */
static void WaitForClientConnection(MockMessageDispatcher *dispatcher)
{
    printf("Please connect client within %d seconds.\n", CLIENT_CONNECTION_TIMEOUT / 1000);

    WaitForResponse(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientConnectedEvent, StatusCodes::CompletedOk);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);
}

/**
 * @brief Start a service and write a value to one of its characteristics.
 *
 *  The client unit test picks the value up and verifies it.
 *
 * @param configuration Configuration of the service to start.
 */
static void TestNotifyCharacteristic(const char * const configuration)
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, configuration);

    WaitForClientConnection(dispatcher);

    printf("Client connected, subscribe to notifications within %d seconds.\n", CLIENT_SUBSCRIPTION_TIME / 1000);
    vTaskDelay(CLIENT_SUBSCRIPTION_TIME / portTICK_PERIOD_MS);

    printf("Writing value to characteristic\n");
    WriteServerDataSet(dispatcher, NOTIFICATION_CHARACTERISTIC_HANDLE, NOTIFICATION_VALUE, CLIENT_CONNECTION_TIMEOUT);

    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Test starting a Bluetooth service for MeadowA, Company ID Espressif (0x02e5).
 */
TEST_CASE("Start Bluetooth (MeadowA, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowA, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_a_json);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Test starting a Bluetooth service for MeadowB, Company ID BaiDu (0x011c).
 */
TEST_CASE("Start Bluetooth (MeadowB, Baidu)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowB, Company ID Baidu (0x011c)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_b_json);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Test starting a Bluetooth service for MeadowC with no company identifier.
 */
TEST_CASE("Notify characteristic (MeadowC, no company identifier)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for (MeadowC, no company identifier)\n");

    TestNotifyCharacteristic(meadow_c_json);
}

/**
 * @brief Test starting a Bluetooth service for MeadowD with no company identifier.
 *
 *  MeadowD raises a client disconnected event alongside the response to the write, so the
 *  queue is drained rather than being checked for a single response.
 */
TEST_CASE("Notify characteristic, multiple descriptors (MeadowD, no company identifier)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for (MeadowD, no company identifier)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_d_json);

    WaitForClientConnection(dispatcher);

    printf("Client connected, subscribe to notifications within %d seconds.\n", CLIENT_SUBSCRIPTION_TIME / 1000);
    vTaskDelay(CLIENT_SUBSCRIPTION_TIME / portTICK_PERIOD_MS);

    printf("Writing value to characteristic\n");
    QueueServerDataSet(dispatcher, NOTIFICATION_CHARACTERISTIC_HANDLE, NOTIFICATION_VALUE);

    //
    //  Wait for the confirmation message that the change has been written back.
    //
    WaitForResponse(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    while (dispatcher->OutboundMessageQueueLength() > 0)
    {
        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);
        ASSERT_NO_PAYLOAD(response);
        if ((response->MessageType != MessageTypes::Response) && (response->MessageType != MessageTypes::Event))
        {
            printf("Unexpected message type: %" PRIu32 "\n", static_cast<uint32_t>(response->MessageType));
            TEST_FAIL();
        }
        TEST_ASSERT_EQUAL(Esp32Interfaces::BlueTooth, response->Interface);
        if ((response->Function != BluetoothFunction::ServerDataSet) && (response->Function != BluetoothFunction::ClientDisconnectedEvent))
        {
            printf("Unexpected function: %" PRIu32 "\n", response->Function);
            TEST_FAIL();
        }
        TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
        Message::DeleteMessage(response);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);
}

/**
 * @brief Test starting a Bluetooth service for MeadowF7, Company ID Espressif (0x02e5).
 */
TEST_CASE("Start Bluetooth (MeadowF7, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowF7 (Bluetooth_Basics), Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_f7_json);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Test stopping the Bluetooth service when no service has been started.
 *
 *  Nothing was started, so the service raises the stopping event and then fails the
 *  request without raising a stopped event.
 */
TEST_CASE("Stop Bluetooth service (no service started)", CLASS_OF_TESTS)
{
    printf("Stopping Bluetooth service\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    RunBluetoothLifecycleRequest(dispatcher, BluetoothFunction::Stop, BluetoothFunction::BluetoothStoppingEvent, NO_COMPLETION_EVENT, StatusCodes::Failure, nullptr, 0);
}

/**
 * @brief Test the stop functionality when Bluetooth has been started.
 */
TEST_CASE("Stop Bluetooth Service (will start MeadowA service first)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowA, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_a_json);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    StopBluetoothService(dispatcher);

    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);

    ResetMessageDispatcher();

    EndOfTest();
}

/**
 * @brief Test restarting the Bluetooth service.
 */
TEST_CASE("Restart Bluetooth service with different configuration", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowA, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_a_json);

    printf("Waiting for 1 second before stopping Bluetooth service\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Stopping Bluetooth service\n");
    StopBluetoothService(dispatcher);

    printf("Starting Bluetooth service for MeadowB, Company ID Baidu (0x011c)\n");
    StartBluetoothService(dispatcher, meadow_b_json);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Test the stop functionality when Bluetooth has been started.
 *
 *  BluetoothRestartIterations in the configuration file says how many times the service
 *  is restarted.  At its default of 50 this takes four or five minutes, so a larger
 *  count wants the time it will take working out before it is asked for.
 */
TEST_CASE("Restart Bluetooth Service repeatedly (test for memory leak)", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    const uint32_t iterations = UnitTestConfiguration::BluetoothRestartIterations();
    for (uint32_t counter = 0; counter < iterations; counter++)
    {
        printf("Iteration %" PRIu32 " of %" PRIu32 "\n", counter + 1, iterations);

        StartBluetoothService(dispatcher, meadow_a_json);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        StopBluetoothService(dispatcher);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        StartBluetoothService(dispatcher, meadow_b_json);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        StopBluetoothService(dispatcher);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ResetMessageDispatcher();

    EndOfTest();
}

/**
 * @brief Test starting a Bluetooth service for MeadowF7, Company ID Espressif (0x02e5) and then accepting
 *        multiple write requests.
 */
TEST_CASE("Soak test writing numeric characteristic, number of iterations controlled by client (MeadowF7, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowF7 (Bluetooth_Basics), Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_f7_json);

    //
    //  The request has to be on the heap, see RunBluetoothLifecycleRequest for why.
    //
    Message *getHandles = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::BlueTooth, BluetoothFunction::GetHandles, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(getHandles);
    dispatcher->QueueMessageForEsp32(getHandles);

    WaitForResponse(dispatcher, BLUETOOTH_RESPONSE_TIMEOUT);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::GetHandles, StatusCodes::CompletedOk);
    ASSERT_HAS_PAYLOAD(response);
    TEST_ASSERT_EQUAL(MEADOW_F7_HANDLES_PAYLOAD_LENGTH, response->PayloadLength);
    //
    //  The characteristic handles for the meadow_f7_json service will be in the following order:
    //      - Service handle
    //      - 016e99d6-8a61-11eb-8dcd-0242ac1300ff (OnOff)
    //      - 017e99d6-8a61-11eb-8dcd-0242ac1300aa (BooleanField)
    //      - 018e99d6-8a61-11eb-8dcd-0242ac1300bb (NumberField)
    //      - 019e99d6-8a61-11eb-8dcd-0242ac1300cc (TextField)
    //
    __attribute__((unused)) uint16_t handleCount = Encoders::ExtractUInt16(response->Payload);
    __attribute__((unused)) uint32_t handlesLength = Encoders::ExtractUInt32(response->Payload + 2);
    __attribute__((unused)) uint16_t numberOfHandles = Encoders::ExtractUInt16(response->Payload + 6);
    __attribute__((unused)) uint16_t serviceHandle = Encoders::ExtractUInt16(response->Payload + 8);
    __attribute__((unused)) uint16_t onOffDeclarationHandle = Encoders::ExtractUInt16(response->Payload + 10);
    __attribute__((unused)) uint16_t onOffValueHandle = Encoders::ExtractUInt16(response->Payload + 12);
    __attribute__((unused)) uint16_t booleanFieldDeclarationHandle = Encoders::ExtractUInt16(response->Payload + 14);
    __attribute__((unused)) uint16_t booleanFieldValueHandle = Encoders::ExtractUInt16(response->Payload + 16);
    uint16_t numberFieldValueHandle = Encoders::ExtractUInt16(response->Payload + 20);
    __attribute__((unused)) uint16_t numberFieldDeclarationHandle = Encoders::ExtractUInt16(response->Payload + 18);
    __attribute__((unused)) uint16_t textFieldDeclarationHandle = Encoders::ExtractUInt16(response->Payload + 22);
    Message::DeleteMessage(response);

    WaitForClientConnection(dispatcher);

    uint32_t iteration = 0;
    while (true)
    {
        iteration++;
        if (((iteration % 100) == 0) || (iteration < 10))
        {
            printf("    Iteration %" PRIu32 "\n", iteration);
        }
        //
        // Wait for a change in the number characteristic.
        //
        WaitForResponse(dispatcher, CLIENT_CONNECTION_TIMEOUT);
        TEST_ASSERT_GREATER_THAN(0, dispatcher->OutboundMessageQueueLength());
        //
        //  We should get the value as an event, the memory will have been allocated by the
        //  BluetoothRequestHandler so we need to release it.
        //
        response = dispatcher->GetOutboundMessage();
        ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, StatusCodes::CompletedOk);
        TEST_ASSERT_NOT_NULL(response->Payload);
        TEST_ASSERT_GREATER_OR_EQUAL(7, response->PayloadLength);

        uint16_t handle = Encoders::ExtractUInt16(response->Payload);
        uint32_t length = Encoders::ExtractUInt32(response->Payload + 2);
        uint32_t value = 0;
        for (uint32_t index = 0; index < length; index++)
        {
            value |= (response->Payload[6 + index] << (index * 8));
        }
        TEST_ASSERT_EQUAL(numberFieldValueHandle, handle);
        Message::DeleteMessage(response);
        //
        //  Use the value to determine if we should stop the test.
        //
        if (value == 0)
        {
            break;
        }
        //
        //  Write the value back to the characteristic for the client unit test to pick up
        //  and verify the value.
        //
        WriteServerDataSet(dispatcher, handle, value, CLIENT_CONNECTION_TIMEOUT);
    }

    //
    //  We should get a client disconnect event.
    //
    WaitForResponse(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    response = dispatcher->GetOutboundMessage();
    ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}
