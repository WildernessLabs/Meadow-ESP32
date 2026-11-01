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
#include <vector>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "MockTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "Logging.hpp"
#include "Utils.hpp"

#include "TestAssertions.hpp"
#include "TestBluetoothHelpers.hpp"
#include "TestEncoders.hpp"
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
 * @brief Number of handles the MeadowA service returns.
 *
 *  The service, plus a declaration and a value handle for each of its two
 *  characteristics, and the Client Characteristic Configuration descriptor the firmware
 *  adds to the second, which notifies (issue #496).
 */
#define MEADOW_A_HANDLE_COUNT 6

/**
 * @brief Number of handles the MeadowF7 service returns.
 *
 *  The handles are all uint16_t values.  The first value is the handle for the service,
 *  and each of the four characteristics contributes the handle for its declaration
 *  followed by the handle for its value.
 */
#define MEADOW_F7_HANDLE_COUNT 9

/**
 * @brief Length of the header in a GetHandles payload.
 *
 *  A uint16_t handle count and a uint32_t length precede the handles.
 */
#define HANDLES_PAYLOAD_HEADER_LENGTH (2 + 4)

/**
 * @brief Index of the NumberField value handle among the MeadowF7 handles.
 *
 *  After the service handle and the declaration and value handles of OnOff and
 *  BooleanField, then the NumberField declaration handle.
 */
#define MEADOW_F7_NUMBER_FIELD_VALUE_INDEX 6

/**
 * @brief Number of handles the MeadowF7NotifyTest service returns.
 *
 *  The service, the declaration and value of the iteration count and the client number,
 *  and the declaration, value and Client Characteristic Configuration descriptor of the
 *  notify field.
 */
#define MEADOW_F7_NOTIFY_HANDLE_COUNT 8

/**
 * @brief Index of the IterationCountField value handle among the MeadowF7NotifyTest handles.
 */
#define MEADOW_F7_NOTIFY_ITERATION_COUNT_VALUE_INDEX 2

/**
 * @brief Index of the ClientNumberField value handle among the MeadowF7NotifyTest handles.
 */
#define MEADOW_F7_NOTIFY_CLIENT_NUMBER_VALUE_INDEX 4

/**
 * @brief Index of the NotifyField value handle among the MeadowF7NotifyTest handles.
 */
#define MEADOW_F7_NOTIFY_NOTIFY_FIELD_VALUE_INDEX 6

/**
 * @brief Value the notify soak client writes to the client number field first.
 *
 *  It says the test is being driven by a script rather than by hand.
 */
#define SCRIPT_CONTROLLED_MARKER 0x7fffffff

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
 * @brief Milliseconds between checks of the outbound queue in the soak test.
 *
 *  WaitForResponse checks once a second, which would limit the soak test to about one
 *  iteration a second.
 */
#define SOAK_POLL_INTERVAL 10

/**
 * @brief Wait for a message to arrive in the outbound queue, or for a timeout.
 *
 * @param dispatcher Dispatcher whose outbound queue is being watched.
 * @param milliseconds Number of milliseconds to wait.
 */
static void WaitForOutboundMessage(MockMessageDispatcher *dispatcher, const uint32_t milliseconds)
{
    uint32_t waited = 0;

    while ((dispatcher->OutboundMessageQueueLength() == 0) && (waited < milliseconds))
    {
        vTaskDelay(pdMS_TO_TICKS(SOAK_POLL_INTERVAL));
        waited += SOAK_POLL_INTERVAL;
    }
}

/**
 * @brief Start a service and write a value to one of its characteristics.
 *
 *  The client picks the value up, verifies it and disconnects.  The disconnection can be
 *  raised before or after the response to the write, so the two are accepted in either
 *  order.  The service is only stopped once the client has gone: stopping it with a
 *  client connected raises a disconnection in the middle of the stop sequence.
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
    QueueServerDataSet(dispatcher, NOTIFICATION_CHARACTERISTIC_HANDLE, NOTIFICATION_VALUE);

    bool responded = false;
    bool disconnected = false;
    while (!(responded && disconnected))
    {
        WaitForResponse(dispatcher, CLIENT_CONNECTION_TIMEOUT);
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, dispatcher->OutboundMessageQueueLength(), responded ? "Client did not disconnect." : "No response to the write.");
        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);
        ASSERT_NO_PAYLOAD(response);
        if (response->Function == BluetoothFunction::ServerDataSet)
        {
            TEST_ASSERT_FALSE_MESSAGE(responded, "More than one response to the write.");
            ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::CompletedOk);
            responded = true;
        }
        else
        {
            TEST_ASSERT_FALSE_MESSAGE(disconnected, "More than one disconnection.");
            ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
            disconnected = true;
        }
        Message::DeleteMessage(response);
    }

    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Check that Start is refused when the configuration cannot fit within a single
 *        message payload, rather than being handed to cJSON unbounded.
 *
 *  Regression check for issue #225.  The configuration here carries no NUL anywhere in
 *  the first Message::MAXIMUM_PAYLOAD_LENGTH bytes, so Encoders::ExtractString cannot find
 *  a terminator within that bound and BuildGraphFromJson is called with a null
 *  configuration.  The graph is never built and, unlike the other tests in this file,
 *  nothing here touches the radio, so it runs under emulation.
 */

//
//  Forward declaration: defined further down, alongside its other callers, but needed
//  by "ServerDataSet refuses an unknown handle and an oversized length" above them.
//
static void GetServiceHandles(MockMessageDispatcher *dispatcher, const uint16_t expectedCount, uint16_t * const handles);

TEST_CASE("Start is refused when the configuration is not shorter than the maximum payload length", "[Bluetooth][Emulated]")
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    const uint32_t payloadLength = Message::MAXIMUM_PAYLOAD_LENGTH + 16;
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    memset(payload, 'x', payloadLength);

    RunBluetoothLifecycleRequest(dispatcher, BluetoothFunction::Start, BluetoothFunction::BluetoothStartingEvent, NO_COMPLETION_EVENT, StatusCodes::InvalidConfigurationFile, payload, payloadLength);

    ResetMessageDispatcher();
}

/**
 * @brief Check that Start is refused, not crashed, when the request carries no payload.
 *
 *  Regression check for issue #390.  Start passed the payload straight to
 *  ExtractBTStackConfig, which reads it without checking it exists, so an empty request
 *  dereferenced a null pointer.  Nothing here touches the radio, so it runs under emulation.
 */
TEST_CASE("Start is refused when the request has no payload", "[Bluetooth][Emulated]")
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    RunBluetoothLifecycleRequest(dispatcher, BluetoothFunction::Start, BluetoothFunction::BluetoothStartingEvent, NO_COMPLETION_EVENT, StatusCodes::InvalidConfigurationFile, nullptr, 0);

    ResetMessageDispatcher();
}

/**
 * @brief Check that Start is refused when an attribute needs MITM protection the security settings do not demand.
 *
 *  No pairing could reach such an attribute (issue #398).  The configuration is refused before
 *  anything touches the radio, so it runs under emulation.
 */
TEST_CASE("Start is refused when an attribute needs MITM protection the security settings do not demand", "[Bluetooth][Emulated]")
{
    static const char *unreachableJson = R"({
    "deviceName":"MeadowMitm",
    "services": [
        {
            "name":"MitmService",
            "uuid":248,
            "characteristics": [ { "uuid":10755, "permission":68, "props":10, "len":4 } ]
        }
    ]
})";

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    Esp32Messaging::BTStackConfig stackConfig = {};
    stackConfig.Config = const_cast<char *>(unreachableJson);
    const uint32_t payloadLength = TestEncoders::EncodedBTStackConfigBufferSize(&stackConfig);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    TestEncoders::EncodeBTStackConfig(&stackConfig, payload);

    RunBluetoothLifecycleRequest(dispatcher, BluetoothFunction::Start, BluetoothFunction::BluetoothStartingEvent, NO_COMPLETION_EVENT, StatusCodes::InvalidConfigurationFile, payload, payloadLength);

    ResetMessageDispatcher();
}

/**
 * @brief Check that ServerDataSet is refused, not crashed, when the request carries no payload.
 *
 *  Regression check for issue #390.  ExtractBTServerDataSet reads the handle straight from
 *  the payload, so an empty request dereferenced a null pointer.  The refusal comes before
 *  anything that needs the Bluetooth stack, so it runs under emulation.
 */
TEST_CASE("ServerDataSet is refused when the request has no payload", "[Bluetooth][Emulated]")
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::Failure, 0, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);
    dispatcher->QueueMessageForEsp32(request);

    Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, BLUETOOTH_RESPONSE_TIMEOUT);
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "The ServerDataSet request was never answered.");
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::InvalidRequest);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);

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
 * @brief Encode and queue a ServerDataSet request with an arbitrary handle and length,
 *        without going through QueueServerDataSet's fixed 4-byte value.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param handle Handle to write to.
 * @param dataLength Value to encode into the wire SetDataLength field.
 * @param data Buffer of at least dataLength bytes: EncodeBTServerDataSet copies exactly
 *        that many bytes from it while building the request, regardless of what the fix
 *        under test goes on to do with the request.
 */
static void QueueServerDataSetRaw(MockMessageDispatcher *dispatcher, const uint16_t handle, const uint32_t dataLength, uint8_t * const data)
{
    Esp32Messaging::BTServerDataSet dataSet = {};
    dataSet.Handle = handle;
    dataSet.SetDataLength = dataLength;
    dataSet.SetData = data;

    const uint32_t payloadLength = TestEncoders::EncodedBTServerDataSetBufferSize(&dataSet);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    TestEncoders::EncodeBTServerDataSet(&dataSet, payload);

    Message *message = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::Failure, 0, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(message);

    dispatcher->QueueMessageForEsp32(message);
}

/**
 * @brief Check that ServerDataSet refuses an unknown handle, rather than passing it to
 *        the manager or the BLE stack.
 *
 *  Regression check for issue #300.  No client is needed: the request is rejected before
 *  anything is sent over the air.
 *
 *  This does not also cover the oversized-length half of #300 (SetDataLength above
 *  UINT16_MAX).  EncodeBTServerDataSet, on the test side, and ExtractBTServerDataSet, on
 *  the firmware side, both copy the full declared length before this fix's check ever
 *  runs, so proving that boundary needs a genuine 64KB+ allocation - measured to fail
 *  with the Bluetooth stack already running on this board ("Mem alloc fail. size
 *  0x00010006"), which lands safely in this fix's existing ds == nullptr /
 *  EspOutOfMemory path rather than reaching the length check at all.  On real hardware
 *  an oversized length is therefore more likely to be caught by that allocation failure
 *  than by the explicit boundary check, which mainly guards the (memory-permitting)
 *  range between "fits in heap" and "fits in uint16_t".
 */
TEST_CASE("ServerDataSet refuses an unknown handle", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_a_json);

    uint16_t handles[MEADOW_A_HANDLE_COUNT];
    GetServiceHandles(dispatcher, MEADOW_A_HANDLE_COUNT, handles);
    const uint16_t unknownHandle = static_cast<uint16_t>(handles[MEADOW_A_HANDLE_COUNT - 1] + 1000);

    uint32_t smallValue = 0;
    QueueServerDataSetRaw(dispatcher, unknownHandle, sizeof(smallValue), reinterpret_cast<uint8_t *>(&smallValue));
    WaitForResponse(dispatcher, BLUETOOTH_RESPONSE_TIMEOUT);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);
    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::InvalidRequest);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);

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
 */
TEST_CASE("Notify characteristic, multiple descriptors (MeadowD, no company identifier)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for (MeadowD, no company identifier)\n");

    TestNotifyCharacteristic(meadow_d_json);
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
 * @brief Send a Start request with the given configuration and check it is answered Failure.
 *
 *  No BluetoothStartedEvent may be raised, then or afterwards, and the Bluetooth stack the
 *  failed start left running must still stop cleanly so later tests can start it again.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param configuration Configuration the Bluetooth stack cannot start a service from.
 */
static void CheckStartReportsFailure(MockMessageDispatcher *dispatcher, const char * const configuration)
{
    Esp32Messaging::BTStackConfig stackConfig = {};
    stackConfig.Config = const_cast<char *>(configuration);

    const uint32_t payloadLength = TestEncoders::EncodedBTStackConfigBufferSize(&stackConfig);
    TEST_ASSERT_GREATER_THAN(0, payloadLength);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    TestEncoders::EncodeBTStackConfig(&stackConfig, payload);

    RunBluetoothLifecycleRequest(dispatcher, BluetoothFunction::Start, BluetoothFunction::BluetoothStartingEvent, NO_COMPLETION_EVENT, StatusCodes::Failure, payload, payloadLength);

    //
    //  A started event arriving after the failure response would contradict it.
    //
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);

    StopBluetoothService(dispatcher);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);
}

/**
 * @brief Start is answered Failure when the stack refuses the attribute table asynchronously.
 *
 *  Bluedroid accepts only one primary service per attribute table, and the handler builds a
 *  single table holding every service, so a configuration with two services is refused in
 *  ESP_GATTS_CREAT_ATTR_TAB_EVT, after StartBluetoothService() has returned.  Start used to
 *  answer CompletedOk and raise BluetoothStartedEvent regardless.
 */
TEST_CASE("Start reports failure when the attribute table is refused (two services)", CLASS_OF_TESTS)
{
    static const char *twoServicesJson = R"({
    "deviceName":"MeadowTwo",
    "companyIdentifier": 741,
    "services": [
        {
            "name": "Service A",
            "uuid": 254,
            "characteristics": [ { "uuid": 10755, "permission": 17, "props": 18, "len": 4 } ]
        },
        {
            "name": "Service B",
            "uuid": 253,
            "characteristics": [ { "uuid": 10756, "permission": 17, "props": 18, "len": 4 } ]
        }
    ]
})";

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    CheckStartReportsFailure(dispatcher, twoServicesJson);

    ResetMessageDispatcher();
    EndOfTest();
}

/**
 * @brief Start is answered Failure when the attribute table is too large for the stack.
 *
 *  Fifty characteristics make a table of 101 attributes (the service plus a declaration and
 *  a value for each), one more than CONFIG_BT_GATT_MAX_SR_ATTRIBUTES allows, so
 *  esp_ble_gatts_create_attr_tab() refuses it in ESP_GATTS_REG_EVT.
 */
TEST_CASE("Start reports failure when the attribute table is too large", CLASS_OF_TESTS)
{
    static constexpr int CHARACTERISTICS = 50;

    std::string configuration = R"({"deviceName":"MeadowBig","companyIdentifier":741,"services":[{"name":"Big service","uuid":254,"characteristics":[)";
    for (int index = 0; index < CHARACTERISTICS; index++)
    {
        if (index > 0)
        {
            configuration += ",";
        }
        configuration += "{\"uuid\":" + std::to_string(0x2a00 + index) + ",\"permission\":17,\"props\":2,\"len\":4}";
    }
    configuration += "]}]}";

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    CheckStartReportsFailure(dispatcher, configuration.c_str());

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

    //
    //  One start/stop cycle before the measurements start, so that anything allocated
    //  once and kept (e.g. lazily-initialised Bluetooth controller state) is not counted
    //  as a leak.
    //
    StartBluetoothService(dispatcher, meadow_a_json);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    StopBluetoothService(dispatcher);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    const uint32_t iterations = UnitTestConfiguration::BluetoothRestartIterations();
    const uint32_t heapAtStart = esp_get_free_heap_size();

    for (uint32_t counter = 0; counter < iterations; counter++)
    {
        printf("Iteration %" PRIu32 " of %" PRIu32 ", free heap %" PRIu32 " bytes.\n", counter + 1, iterations, esp_get_free_heap_size());

        StartBluetoothService(dispatcher, meadow_a_json);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        StopBluetoothService(dispatcher);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        StartBluetoothService(dispatcher, meadow_b_json);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        StopBluetoothService(dispatcher);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    const uint32_t heapAtEnd = esp_get_free_heap_size();

    printf("Free heap %" PRIu32 " bytes at the start, %" PRIu32 " at the end.\n", heapAtStart, heapAtEnd);

    //
    //  The heap is not expected to come back to exactly where it started, because
    //  allocation patterns leave it slightly differently arranged, but a real leak over
    //  this many restarts is far larger than the allowance the configuration gives.
    //
    if (heapAtEnd < heapAtStart)
    {
        const uint32_t allowance = UnitTestConfiguration::SoakHeapAllowance();
        const uint32_t lost = heapAtStart - heapAtEnd;
        printf("Free heap fell by %" PRIu32 " bytes over %" PRIu32 " restarts, against an allowance of %" PRIu32 ".\n", lost, iterations, allowance);
        TEST_ASSERT_TRUE_MESSAGE(lost <= allowance, "The free heap fell too far, which suggests a leak in the Bluetooth attribute teardown path.");
    }

    ResetMessageDispatcher();

    EndOfTest();
}

/**
 * @brief Free heap and timing across the measured part of a soak test.
 *
 *  The first measurement is the baseline, so whatever the soak allocates once and keeps
 *  before then is not counted as a leak.  Each later measurement becomes the end figure.
 *  Measurements are only taken where nothing is in flight, so that the start and the end
 *  are taken in the same state.
 */
class SoakHeapMonitor
{
private:
    /**
     * @brief True once the baseline has been taken.
     */
    bool _baselineTaken = false;

    /**
     * @brief Iteration at which the baseline was taken.
     */
    uint32_t _baselineIteration = 0;

    /**
     * @brief Free heap at the baseline.
     */
    uint32_t _heapAtStart = 0;

    /**
     * @brief Free heap at the most recent measurement.
     */
    uint32_t _heapAtEnd = 0;

    /**
     * @brief Time the baseline was taken, in microseconds.
     */
    int64_t _startedAt = 0;

    /**
     * @brief Iterations between progress reports, zero for none.
     */
    const uint32_t _progressInterval = UnitTestConfiguration::SoakProgressInterval();

public:
    /**
     * @brief Record the free heap at a point where nothing is in flight.
     *
     * @param iteration Iterations completed so far.
     */
    void Measure(const uint32_t iteration)
    {
        _heapAtEnd = esp_get_free_heap_size();
        if (!_baselineTaken)
        {
            _baselineTaken = true;
            _baselineIteration = iteration;
            _heapAtStart = _heapAtEnd;
            _startedAt = esp_timer_get_time();
            printf("Warm-up complete after %" PRIu32 " iterations, free heap %" PRIu32 " bytes.\n", iteration, _heapAtStart);
        }
    }

    /**
     * @brief Report the free heap and the rate if this iteration is due a report.
     *
     * @param iteration Iterations completed so far.
     */
    void ReportProgress(const uint32_t iteration) const
    {
        if ((_progressInterval > 0) && ((iteration % _progressInterval) == 0) && _baselineTaken && (iteration > _baselineIteration))
        {
            const int64_t elapsed = esp_timer_get_time() - _startedAt;
            printf("  %" PRIu32 " iterations, free heap %" PRIu32 " bytes, lowest %" PRIu32 ", %" PRId64 " ms per iteration.\n", iteration, esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), (elapsed / 1000) / (iteration - _baselineIteration));
        }
    }

    /**
     * @brief Report the totals and fail if the free heap fell further than the allowance.
     *
     *  The heap is not expected to come back to exactly where it started, because
     *  allocation patterns leave it slightly differently arranged, but a real leak over
     *  this many round trips is far larger than the allowance the configuration gives.  A
     *  run that never got past the warm-up has nothing to compare.
     *
     * @param iteration Iterations completed.
     * @param failureMessage Message to fail with when the heap fell too far.
     */
    void Check(const uint32_t iteration, const char * const failureMessage) const
    {
        if (!_baselineTaken || (iteration <= _baselineIteration))
        {
            printf("Too few iterations after the warm-up to check the heap.\n");
            return;
        }

        const uint32_t measured = iteration - _baselineIteration;
        const int64_t elapsed = esp_timer_get_time() - _startedAt;
        printf("Measured %" PRIu32 " iterations in %" PRId64 " seconds.\n", measured, elapsed / 1000000);
        printf("Free heap %" PRIu32 " bytes at the start, %" PRIu32 " at the end, lowest %" PRIu32 ".\n", _heapAtStart, _heapAtEnd, esp_get_minimum_free_heap_size());

        if (_heapAtEnd < _heapAtStart)
        {
            const uint32_t allowance = UnitTestConfiguration::SoakHeapAllowance();
            const uint32_t lost = _heapAtStart - _heapAtEnd;
            printf("Free heap fell by %" PRIu32 " bytes over %" PRIu32 " iterations, against an allowance of %" PRIu32 ".\n", lost, measured, allowance);
            TEST_ASSERT_TRUE_MESSAGE(lost <= allowance, failureMessage);
        }
    }
};

/**
 * @brief Ask for the handles of the running service and check how many there are.
 *
 *  The payload is the handle count (2 bytes) and the length of the handle list (4 bytes),
 *  followed by the handles themselves, two bytes each: the service, then for each
 *  characteristic its declaration, its value and any descriptors, in configuration order.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param expectedCount Number of handles the service should have.
 * @param handles Array of at least expectedCount entries to receive the handles.
 */
static void GetServiceHandles(MockMessageDispatcher *dispatcher, const uint16_t expectedCount, uint16_t * const handles)
{
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
    TEST_ASSERT_EQUAL(HANDLES_PAYLOAD_HEADER_LENGTH + (expectedCount * sizeof(uint16_t)), response->PayloadLength);

    const uint16_t handleCount = Encoders::ExtractUInt16(response->Payload);
    const uint32_t handlesLength = Encoders::ExtractUInt32(response->Payload + 2);
    TEST_ASSERT_EQUAL(expectedCount, handleCount);
    TEST_ASSERT_EQUAL(expectedCount * sizeof(uint16_t), handlesLength);
    for (uint16_t index = 0; index < expectedCount; index++)
    {
        handles[index] = Encoders::ExtractUInt16(response->Payload + HANDLES_PAYLOAD_HEADER_LENGTH + (index * sizeof(uint16_t)));
    }
    Message::DeleteMessage(response);
}

/**
 * @brief Check a client write event and extract the four byte value written.
 *
 *  The event's memory was allocated by the BluetoothRequestHandler, and it is released
 *  here.
 *
 * @param event Event raised for the write.
 * @param handle Receives the handle written to.
 * @param value Receives the value written, little endian on the wire.
 */
static void ExtractClientWrite(Message *event, uint16_t &handle, uint32_t &value)
{
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, StatusCodes::CompletedOk);
    TEST_ASSERT_NOT_NULL(event->Payload);
    TEST_ASSERT_GREATER_OR_EQUAL(6 + sizeof(uint32_t), event->PayloadLength);

    handle = Encoders::ExtractUInt16(event->Payload);
    const uint32_t length = Encoders::ExtractUInt32(event->Payload + 2);
    TEST_ASSERT_EQUAL(sizeof(uint32_t), length);
    value = 0;
    for (uint32_t index = 0; index < length; index++)
    {
        value |= (static_cast<uint32_t>(event->Payload[6 + index]) << (index * 8));
    }
    Message::DeleteMessage(event);
}

/**
 * @brief Test starting a Bluetooth service for MeadowF7, Company ID Espressif (0x02e5) and then accepting
 *        multiple write requests.
 *
 *  WriteNumberCharacteristicSoakTest.py is the client.  It writes 1, 2, 3 and so on to the
 *  number characteristic, reading each value back once it has been echoed, and writes 0
 *  to finish.
 */
TEST_CASE("Soak test writing numeric characteristic, number of iterations controlled by client (MeadowF7, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowF7 (Bluetooth_Basics), Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_f7_json);

    uint16_t handles[MEADOW_F7_HANDLE_COUNT];
    GetServiceHandles(dispatcher, MEADOW_F7_HANDLE_COUNT, handles);
    const uint16_t numberFieldValueHandle = handles[MEADOW_F7_NUMBER_FIELD_VALUE_INDEX];

    WaitForClientConnection(dispatcher);

    //
    //  The client sees the echoed value as soon as the stack has it, which can be before
    //  the response to the ServerDataSet request reaches the outbound queue.  A prompt
    //  client therefore writes its next value first, so the response and the next write
    //  are accepted in either order rather than one strictly after the other.
    //
    SoakHeapMonitor monitor;
    uint32_t iteration = 0;
    uint32_t outstandingDataSetResponses = 0;
    bool clientFinished = false;
    bool clientDisconnected = false;
    while (!clientDisconnected)
    {
        WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
        TEST_ASSERT_GREATER_THAN(0, dispatcher->OutboundMessageQueueLength());
        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);

        if (response->Function == BluetoothFunction::ServerDataSet)
        {
            ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::CompletedOk);
            ASSERT_NO_PAYLOAD(response);
            TEST_ASSERT_GREATER_THAN_MESSAGE(0, outstandingDataSetResponses, "ServerDataSet response with no request outstanding.");
            outstandingDataSetResponses--;
            Message::DeleteMessage(response);
            if (outstandingDataSetResponses == 0)
            {
                monitor.Measure(iteration);
            }
            continue;
        }

        if (clientFinished)
        {
            //
            //  Once the client has written 0 the only other thing it should do is leave.
            //
            ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
            ASSERT_NO_PAYLOAD(response);
            Message::DeleteMessage(response);
            clientDisconnected = true;
            continue;
        }

        uint16_t handle = 0;
        uint32_t value = 0;
        ExtractClientWrite(response, handle, value);
        TEST_ASSERT_EQUAL(numberFieldValueHandle, handle);

        //
        //  The client writes 0 to say the test is over.
        //
        if (value == 0)
        {
            clientFinished = true;
            continue;
        }

        iteration++;
        TEST_ASSERT_EQUAL_UINT32(iteration, value);
        monitor.ReportProgress(iteration);

        //
        //  Write the value back to the characteristic for the client to read and verify.
        //
        QueueServerDataSet(dispatcher, handle, value);
        outstandingDataSetResponses++;
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, outstandingDataSetResponses, "ServerDataSet requests left without a response.");
    printf("Client completed %" PRIu32 " iterations.\n", iteration);

    monitor.Check(iteration, "The free heap fell too far, which suggests a leak in the characteristic write path.");

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Length of the long value WriteDeliveryClient.py writes, longer than one packet.
 */
#define WRITE_DELIVERY_LONG_LENGTH 512

/**
 * @brief Check that a write without response and a long write both reach the STM32.
 *
 *  WriteDeliveryClient.py is the client.  It writes 11 22 33 44 without asking for a
 *  response, then writes WRITE_DELIVERY_LONG_LENGTH bytes, (index * 3 + 1) & 0xff, with a
 *  response.  The long value does not fit in one packet, so the client's stack sends it as
 *  prepared writes followed by an execute.  Before issue #299 neither reached the STM32.
 */
TEST_CASE("A write without response and a long write reach the STM32 (MeadowWriteTest, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowWriteTest, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_write_json);
    WaitForClientConnection(dispatcher);

    const uint8_t shortValue[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t longValue[WRITE_DELIVERY_LONG_LENGTH];
    for (uint32_t index = 0; index < sizeof(longValue); index++)
    {
        longValue[index] = static_cast<uint8_t>((index * 3) + 1);
    }
    const uint8_t *expectedValues[] = {shortValue, longValue};
    const uint32_t expectedLengths[] = {sizeof(shortValue), sizeof(longValue)};
    const char *descriptions[] = {"The write without response", "The long write"};

    for (uint32_t write = 0; write < 2; write++)
    {
        WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
        Message *event = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL_MESSAGE(event, descriptions[write]);
        if (event->Function != BluetoothFunction::ClientWriteRequestEvent)
        {
            printf("%s did not reach the STM32; event %" PRIu32 " came instead.\n", descriptions[write], event->Function);
        }
        ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, StatusCodes::CompletedOk);
        const uint32_t length = Encoders::ExtractUInt32(event->Payload + 2);
        printf("%s delivered %" PRIu32 " bytes.\n", descriptions[write], length);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(expectedLengths[write], length, descriptions[write]);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expectedValues[write], event->Payload + 6, length, descriptions[write]);
        Message::DeleteMessage(event);
    }

    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(event);
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
    Message::DeleteMessage(event);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Length of the value ReadClient.py reads, longer than a read returns at the 500 byte MTU.
 */
#define LONG_READ_LENGTH 517

/**
 * @brief Number of handles the MeadowReadTest service returns: the service, and the declaration and value of its characteristic.
 */
#define LONG_READ_HANDLE_COUNT 3

/**
 * @brief Index, among the MeadowReadTest handles, of the characteristic's value.
 */
#define LONG_READ_VALUE_INDEX 2

/**
 * @brief Check that a client reading a value longer than one packet gets all of it.
 *
 *  The firmware sets LONG_READ_LENGTH bytes, (index * 7 + 3) & 0xff, on the characteristic.
 *  LongReadClient.py reads it and compares what comes back.  The first read returns what
 *  fits in the MTU and the rest follows as Read Blob Requests at increasing offsets, each
 *  of which used to be answered with the start of the value again (issue #528).  517 bytes
 *  is more than a read returns at the firmware's 500 byte MTU, so this holds whatever MTU
 *  the client negotiates.
 */
TEST_CASE("A value longer than the MTU is read in full (MeadowReadTest, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowReadTest, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_read_json);

    uint16_t handles[LONG_READ_HANDLE_COUNT];
    GetServiceHandles(dispatcher, LONG_READ_HANDLE_COUNT, handles);

    uint8_t value[LONG_READ_LENGTH];
    for (uint32_t index = 0; index < sizeof(value); index++)
    {
        value[index] = static_cast<uint8_t>((index * 7) + 3);
    }
    QueueServerDataSetRaw(dispatcher, handles[LONG_READ_VALUE_INDEX], sizeof(value), value);
    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *response = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::CompletedOk);
    Message::DeleteMessage(response);

    WaitForClientConnection(dispatcher);

    //
    //  The client checks the value and leaves.
    //
    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(event);
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
    Message::DeleteMessage(event);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Number of handles the MeadowSubscribe service returns.
 *
 *  The service, the declaration, value and client configuration descriptor of the notifying
 *  characteristic, and the declaration and value of the characteristic the client writes to
 *  say it has finished a step.
 */
#define SUBSCRIPTION_HANDLE_COUNT 6

/**
 * @brief Indexes, among the MeadowSubscribe handles, of the notifying value and the step value.
 */
#define SUBSCRIPTION_NOTIFY_VALUE_INDEX 2
#define SUBSCRIPTION_STEP_VALUE_INDEX 5

/**
 * @brief Number of steps the subscription client works through.
 */
#define SUBSCRIPTION_STEPS 4

/**
 * @brief Wait for the client to write the number of the step it has finished.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 * @param stepHandle Handle the client writes the step number to.
 * @param step Number of the step expected.
 */
static void WaitForSubscriptionStep(MockMessageDispatcher *dispatcher, const uint16_t stepHandle, const uint32_t step)
{
    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "The client did not report its step.");
    uint16_t handle = 0;
    uint32_t value = 0;
    ExtractClientWrite(event, handle, value);
    TEST_ASSERT_EQUAL_UINT16(stepHandle, handle);
    TEST_ASSERT_EQUAL_UINT32(step, value);
}

/**
 * @brief Set the value of the notifying characteristic, wait for the response and check whether a notification was sent.
 *
 *  A client that has not subscribed never shows that a notification was sent to it, because
 *  CoreBluetooth drops one for a characteristic it is not subscribed to.  So the firmware's
 *  own count of notifications sent is checked, along with the client configuration the
 *  stack holds.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 * @param valueHandle Handle of the notifying characteristic's value.
 * @param value Value to set.
 * @param subscribed Whether the client is subscribed, so whether a notification must be sent.
 */
static void SetSubscriptionValue(MockMessageDispatcher *dispatcher, const uint16_t valueHandle, const uint32_t value, const bool subscribed)
{
    const uint32_t sentBefore = BluetoothRequestHandler::NotificationsSent();
    TEST_ASSERT_EQUAL_MESSAGE(subscribed, BluetoothRequestHandler::NotificationsEnabled(valueHandle), subscribed ? "The client subscribed but its client configuration does not say so." : "The client configuration says the client is subscribed when it is not.");

    QueueServerDataSet(dispatcher, valueHandle, value);
    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *response = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "No response to the write.");
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::CompletedOk);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(sentBefore + (subscribed ? 1 : 0), BluetoothRequestHandler::NotificationsSent(), subscribed ? "No notification was sent to a subscribed client." : "A notification was sent to a client that is not subscribed.");
}

/**
 * @brief Wait for the client to disconnect.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 */
static void WaitForClientDisconnection(MockMessageDispatcher *dispatcher)
{
    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "The client did not disconnect.");
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
    Message::DeleteMessage(event);
}

/**
 * @brief Check that a value is notified only to a client that has subscribed (issue #572).
 *
 *  SubscriptionClient.py is the client.  It tells the firmware each time it has finished a
 *  step by writing the step number, and the firmware then sets the notifying
 *  characteristic's value to the step number repeated in every nibble (0x11111111, 0x22222222
 *  and so on):
 *
 *      1. Not subscribed.  No notification, but the value can be read.
 *      2. Subscribed.  The notification arrives.
 *      3. Unsubscribed again.  No notification, and the value can be read.
 *      4. The client subscribes and disconnects without unsubscribing, then connects again
 *         and does not subscribe.  No notification: a new connection starts unsubscribed.
 *
 *  The reconnection is waited for without the prompt WaitForClientConnection prints, because
 *  TestRunner starts a client every time it sees that prompt.
 */
TEST_CASE("Notifications are sent only to a client that has subscribed (MeadowSubscribe, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowSubscribe, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_subscription_json);

    uint16_t handles[SUBSCRIPTION_HANDLE_COUNT];
    GetServiceHandles(dispatcher, SUBSCRIPTION_HANDLE_COUNT, handles);
    const uint16_t valueHandle = handles[SUBSCRIPTION_NOTIFY_VALUE_INDEX];
    const uint16_t stepHandle = handles[SUBSCRIPTION_STEP_VALUE_INDEX];

    WaitForClientConnection(dispatcher);

    //
    //  Only step 2 has the client subscribed.
    //
    for (uint32_t step = 1; step <= 3; step++)
    {
        WaitForSubscriptionStep(dispatcher, stepHandle, step);
        SetSubscriptionValue(dispatcher, valueHandle, step * 0x11111111, step == 2);
    }

    WaitForClientDisconnection(dispatcher);
    printf("Waiting for the client to connect again.\n");
    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *connected = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(connected, "The client did not connect again.");
    ASSERT_EVENT(connected, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientConnectedEvent, StatusCodes::CompletedOk);
    Message::DeleteMessage(connected);

    WaitForSubscriptionStep(dispatcher, stepHandle, SUBSCRIPTION_STEPS);
    SetSubscriptionValue(dispatcher, valueHandle, SUBSCRIPTION_STEPS * 0x11111111, false);

    WaitForClientDisconnection(dispatcher);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 0);

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Soak test the notification path: the device notifies a number, the client
 *        writes it back, and the device notifies the next one.
 *
 *  NotifyCharacteristicSoakTest.py is the client.  It subscribes to the notify field,
 *  writes SCRIPT_CONTROLLED_MARKER to the client number field and then the number of
 *  iterations to the iteration count field, reading each back once it has been echoed.
 *  The device then notifies 1, 2, 3 and so on, each only once the client has written the
 *  previous one back, and notifies 0 to finish, at which point the client disconnects.
 *
 *  The response to a notification and the client's reply to it can reach the queue in
 *  either order, so neither is required to come first.
 */
TEST_CASE("Soak test notifying characteristic, number of iterations controlled by client (MeadowF7NotifyTest, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowF7NotifyTest, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_f7_notify_json);

    uint16_t handles[MEADOW_F7_NOTIFY_HANDLE_COUNT];
    GetServiceHandles(dispatcher, MEADOW_F7_NOTIFY_HANDLE_COUNT, handles);
    const uint16_t iterationCountHandle = handles[MEADOW_F7_NOTIFY_ITERATION_COUNT_VALUE_INDEX];
    const uint16_t clientNumberHandle = handles[MEADOW_F7_NOTIFY_CLIENT_NUMBER_VALUE_INDEX];
    const uint16_t notifyFieldHandle = handles[MEADOW_F7_NOTIFY_NOTIFY_FIELD_VALUE_INDEX];

    WaitForClientConnection(dispatcher);

    SoakHeapMonitor monitor;
    bool markerReceived = false;
    uint32_t iterations = 0;
    uint32_t notified = 0;
    uint32_t acknowledged = 0;
    uint32_t outstandingDataSetResponses = 0;
    bool endSent = false;
    bool clientDisconnected = false;
    while (!clientDisconnected)
    {
        WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
        TEST_ASSERT_GREATER_THAN(0, dispatcher->OutboundMessageQueueLength());
        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);

        if (response->Function == BluetoothFunction::ServerDataSet)
        {
            ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::CompletedOk);
            ASSERT_NO_PAYLOAD(response);
            TEST_ASSERT_GREATER_THAN_MESSAGE(0, outstandingDataSetResponses, "ServerDataSet response with no request outstanding.");
            outstandingDataSetResponses--;
            Message::DeleteMessage(response);
            continue;
        }

        if (response->Function == BluetoothFunction::ClientDisconnectedEvent)
        {
            ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
            ASSERT_NO_PAYLOAD(response);
            Message::DeleteMessage(response);
            TEST_ASSERT_TRUE_MESSAGE(endSent, "The client disconnected before the end of the test was notified.");
            clientDisconnected = true;
            continue;
        }

        uint16_t handle = 0;
        uint32_t value = 0;
        ExtractClientWrite(response, handle, value);

        if (handle == iterationCountHandle)
        {
            TEST_ASSERT_TRUE_MESSAGE(markerReceived, "Iteration count written before the client number marker.");
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, iterations, "Iteration count written more than once.");
            TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0, value, "The client asked for no iterations.");
            iterations = value;
            printf("Client asked for %" PRIu32 " notifications.\n", iterations);

            //
            //  Echo the count so the client can read it back, then start.
            //
            QueueServerDataSet(dispatcher, handle, value);
            notified = 1;
            QueueServerDataSet(dispatcher, notifyFieldHandle, notified);
            outstandingDataSetResponses += 2;
            continue;
        }

        TEST_ASSERT_EQUAL_MESSAGE(clientNumberHandle, handle, "Write to an unexpected characteristic.");
        if (iterations == 0)
        {
            //
            //  Before the count arrives the only write expected is the marker, echoed so
            //  the client can read it back.
            //
            TEST_ASSERT_EQUAL_HEX32_MESSAGE(SCRIPT_CONTROLLED_MARKER, value, "Expected the client number marker.");
            markerReceived = true;
            QueueServerDataSet(dispatcher, handle, value);
            outstandingDataSetResponses++;
            continue;
        }

        TEST_ASSERT_FALSE_MESSAGE(endSent, "Client number written after the end of the test was notified.");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(notified, value, "The client wrote back a different number from the one notified.");
        acknowledged = value;
        monitor.ReportProgress(acknowledged);
        if (outstandingDataSetResponses == 0)
        {
            monitor.Measure(acknowledged);
        }

        if (acknowledged < iterations)
        {
            notified++;
            QueueServerDataSet(dispatcher, notifyFieldHandle, notified);
        }
        else
        {
            QueueServerDataSet(dispatcher, notifyFieldHandle, 0);
            endSent = true;
        }
        outstandingDataSetResponses++;
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, outstandingDataSetResponses, "ServerDataSet requests left without a response.");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(iterations, acknowledged, "The client did not return every notification.");
    printf("Client returned %" PRIu32 " notifications.\n", acknowledged);

    monitor.Check(acknowledged, "The free heap fell too far, which suggests a leak in the notification path.");

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}

/**
 * @brief Send a bond request through the dispatcher and return its response.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param function GetBondedDevices, RemoveBond or RemoveAllBonds.
 * @param address Address the request is about, nullptr for a request without one.
 *
 * @return The response, which the caller releases.
 */
static Message *SendBondRequest(MockMessageDispatcher *dispatcher, const uint32_t function, const uint8_t * const address)
{
    uint8_t *payload = nullptr;
    uint32_t payloadLength = 0;
    if (address != nullptr)
    {
        payloadLength = Esp32Messaging::BT_ADDRESS_LENGTH;
        payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        TEST_ASSERT_NOT_NULL(payload);
        memcpy(payload, address, payloadLength);
    }
    Message *response = SendRequest(dispatcher, Esp32Interfaces::BlueTooth, function, payload, payloadLength, BLUETOOTH_RESPONSE_TIMEOUT);
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "A bond request was not answered.");
    return (response);
}

/**
 * @brief Get the bonded devices and check the response is a whole list.
 *
 * @param dispatcher Dispatcher to queue the request on.
 *
 * @return The entries: each device's address followed by its type.
 */
static std::vector<uint8_t> GetBondedDevices(MockMessageDispatcher *dispatcher)
{
    Message *response = SendBondRequest(dispatcher, BluetoothFunction::GetBondedDevices, nullptr);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::GetBondedDevices, StatusCodes::CompletedOk);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(6, response->PayloadLength);
    const uint16_t count = Encoders::ExtractUInt16(response->Payload);
    const uint32_t length = Encoders::ExtractUInt32(response->Payload + 2);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(count * Esp32Messaging::BT_BONDED_DEVICE_ENTRY_LENGTH, length, "The list length does not match the number of devices.");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(6 + length, response->PayloadLength, "The payload does not hold the whole list.");
    printf("%" PRIu16 " bonded devices.\n", count);
    const std::vector<uint8_t> entries(response->Payload + 6, response->Payload + 6 + length);
    Message::DeleteMessage(response);
    return (entries);
}

/**
 * @brief Is an address in a list of bonded devices?
 *
 * @param entries Entries from GetBondedDevices.
 * @param address Address to look for.
 *
 * @return true if it is in the list.
 */
static bool ListHolds(const std::vector<uint8_t> &entries, const uint8_t * const address)
{
    for (size_t index = 0; (index + Esp32Messaging::BT_BONDED_DEVICE_ENTRY_LENGTH) <= entries.size(); index += Esp32Messaging::BT_BONDED_DEVICE_ENTRY_LENGTH)
    {
        if (memcmp(entries.data() + index, address, Esp32Messaging::BT_ADDRESS_LENGTH) == 0)
        {
            return (true);
        }
    }
    return (false);
}

/**
 * @brief Start applies every security setting, and bonds can be listed and removed (issue #398).
 *
 *  Start fails if the stack refuses any of the security settings, so starting MeadowBond, which
 *  uses all of them, shows they were taken.  No client is needed.  RemoveAllBonds removes every
 *  bond the board holds, including one a client made in "A client pairs and bonds..."; remove
 *  MeadowPair from that client's Bluetooth settings before pairing it again.
 */
TEST_CASE("Start applies every security setting, and bonds can be listed and removed (MeadowBond, Espressif)", CLASS_OF_TESTS)
{
    printf("Starting Bluetooth service for MeadowBond, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_bond_json);

    GetBondedDevices(dispatcher);

    const uint8_t stranger[Esp32Messaging::BT_ADDRESS_LENGTH] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    Message *response = SendBondRequest(dispatcher, BluetoothFunction::RemoveBond, stranger);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::RemoveBond, StatusCodes::BluetoothDeviceNotBonded);
    Message::DeleteMessage(response);

    response = SendBondRequest(dispatcher, BluetoothFunction::RemoveAllBonds, nullptr);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::RemoveAllBonds, StatusCodes::CompletedOk);
    Message::DeleteMessage(response);
    TEST_ASSERT_EQUAL_MESSAGE(0, GetBondedDevices(dispatcher).size(), "Bonds were left after RemoveAllBonds.");

    StopBluetoothService(dispatcher);

    //
    //  The bonds are kept by the stack, so with it stopped they cannot be listed.
    //
    response = SendBondRequest(dispatcher, BluetoothFunction::GetBondedDevices, nullptr);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::GetBondedDevices, StatusCodes::BluetoothNotStarted);
    Message::DeleteMessage(response);

    ResetMessageDispatcher();
}

/**
 * @brief Value the pairing test puts in the encrypted characteristic for the client to read: "PAIR".
 */
#define PAIRING_VALUE 0x52494150

/**
 * @brief Number of handles the MeadowPair service returns: service, declaration, value and client configuration.
 */
#define PAIRING_HANDLE_COUNT 4

/**
 * @brief Index of the encrypted characteristic's value among the MeadowPair handles.
 */
#define PAIRING_VALUE_INDEX 2

/**
 * @brief Follow one connection of the pairing client until it disconnects.
 *
 *  The link must be encrypted, with the client bonded, before the client writes the number of
 *  the connection to the characteristic.  When it does, it must be subscribed to notifications:
 *  in the first connection because it subscribed, in the second because a bonded client's
 *  subscription is kept from one connection to the next.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 * @param valueHandle Handle of the encrypted characteristic's value.
 * @param connection Number of the connection, 1 or 2, which the client writes.
 * @param address Set to the address the client is bonded with.
 */
static void FollowPairingConnection(MockMessageDispatcher *dispatcher, const uint16_t valueHandle, const uint32_t connection, uint8_t * const address)
{
    bool authenticated = false;
    bool written = false;
    bool disconnected = false;
    while (!disconnected)
    {
        WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
        Message *event = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL_MESSAGE(event, "The client did not finish the connection.");
        switch (event->Function)
        {
            case BluetoothFunction::AuthenticationCompleteEvent:
                ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::AuthenticationCompleteEvent, StatusCodes::CompletedOk);
                TEST_ASSERT_EQUAL_UINT32(Esp32Messaging::BT_ADDRESS_LENGTH + 4, event->PayloadLength);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, event->Payload[9], "The client is not in the bond list.");
                memcpy(address, event->Payload, Esp32Messaging::BT_ADDRESS_LENGTH);
                printf("Connection %" PRIu32 ": encrypted, authentication mode 0x%02" PRIx8 ".\n", connection, event->Payload[7]);
                authenticated = true;
                break;
            case BluetoothFunction::ClientWriteRequestEvent:
                {
                    TEST_ASSERT_TRUE_MESSAGE(authenticated, "The encrypted characteristic was written before the link was encrypted.");
                    uint16_t handle = 0;
                    uint32_t value = 0;
                    ExtractClientWrite(event, handle, value);
                    event = nullptr;
                    TEST_ASSERT_EQUAL_UINT16(valueHandle, handle);
                    TEST_ASSERT_EQUAL_UINT32_MESSAGE(connection, value, "The client wrote the wrong connection number.");
                    TEST_ASSERT_TRUE_MESSAGE(BluetoothRequestHandler::NotificationsEnabled(valueHandle), (connection == 1) ? "The client's subscription was not recorded." : "The bonded client's subscription was not restored when it reconnected.");
                    written = true;
                }
                break;
            case BluetoothFunction::ClientDisconnectedEvent:
                disconnected = true;
                break;
            default:
                printf("Unexpected Bluetooth message, function %" PRIu32 ".\n", event->Function);
                TEST_FAIL_MESSAGE("Unexpected message during the pairing connection.");
                break;
        }
        if (event != nullptr)
        {
            Message::DeleteMessage(event);
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(authenticated, "The link was never encrypted.");
    TEST_ASSERT_TRUE_MESSAGE(written, "The client did not write the encrypted characteristic.");
}

/**
 * @brief A client pairs, bonds, and keeps its subscription when it reconnects (issue #398).
 *
 *  PairingClient.py is the client.  It reads the characteristic, which needs encryption, so the
 *  client pairs (Just Works, with bonding).  The operating system may ask for the pairing to be
 *  confirmed, which is why this test is not in the [Bluetooth] tag: click Connect or Pair when
 *  asked.  The client then subscribes, writes 1, disconnects, reconnects without subscribing and
 *  writes 2.
 *
 *  The bond is kept, so the test can be run again: the client then encrypts with its stored keys
 *  rather than pairing.  If the board's bonds have been removed since (see MeadowBond), remove
 *  MeadowPair from the client's Bluetooth settings first.
 */
TEST_CASE("A client pairs and bonds to use an encrypted characteristic (MeadowPair, Espressif)", "[BluetoothPairing][HardwareOnly]")
{
    printf("Starting Bluetooth service for MeadowPair, Company ID Espressif (0x02e5)\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    StartBluetoothService(dispatcher, meadow_pair_json);

    uint16_t handles[PAIRING_HANDLE_COUNT];
    GetServiceHandles(dispatcher, PAIRING_HANDLE_COUNT, handles);
    const uint16_t valueHandle = handles[PAIRING_VALUE_INDEX];

    QueueServerDataSet(dispatcher, valueHandle, PAIRING_VALUE);
    Message *response = WaitForResponseTo(dispatcher, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, BLUETOOTH_RESPONSE_TIMEOUT);
    TEST_ASSERT_NOT_NULL(response);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::CompletedOk);
    Message::DeleteMessage(response);

    //
    //  Not WaitForClientConnection, which needs the connection to be the only message queued: a
    //  client that is already bonded encrypts as soon as it connects, so the authentication event
    //  can be queued alongside it.  The prompt is the same, as TestRunner starts the client on it.
    //
    printf("Please connect client within %d seconds.\n", CLIENT_CONNECTION_TIMEOUT / 1000);
    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *firstConnection = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(firstConnection, "The client did not connect.");
    ASSERT_EVENT(firstConnection, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientConnectedEvent, StatusCodes::CompletedOk);
    Message::DeleteMessage(firstConnection);
    uint8_t address[Esp32Messaging::BT_ADDRESS_LENGTH] = {};
    FollowPairingConnection(dispatcher, valueHandle, 1, address);
    TEST_ASSERT_FALSE_MESSAGE(BluetoothRequestHandler::NotificationsEnabled(valueHandle), "The subscription was left switched on with no client connected.");

    printf("Waiting for the client to connect again.\n");
    WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
    Message *connected = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(connected, "The client did not connect again.");
    ASSERT_EVENT(connected, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientConnectedEvent, StatusCodes::CompletedOk);
    Message::DeleteMessage(connected);
    uint8_t reconnected[Esp32Messaging::BT_ADDRESS_LENGTH] = {};
    FollowPairingConnection(dispatcher, valueHandle, 2, reconnected);
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(address, reconnected, sizeof(address), "The client came back under a different bonded address.");

    TEST_ASSERT_TRUE_MESSAGE(ListHolds(GetBondedDevices(dispatcher), address), "The client is not in the bond list.");

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}
