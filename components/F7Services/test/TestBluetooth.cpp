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
 * @brief Number of handles the MeadowF7 service returns.
 *
 *  The handles are all uint16_t values.  The first value is the handle for the service,
 *  and each of the four characteristics contributes the handle for its declaration
 *  followed by the handle for its value.
 */
#define MEADOW_F7_HANDLE_COUNT 9

/**
 * @brief Length of the GetHandles payload for the MeadowF7 service.
 *
 *  A uint16_t handle count and a uint32_t length precede the handles.
 */
#define MEADOW_F7_HANDLES_PAYLOAD_LENGTH (2 + 4 + (MEADOW_F7_HANDLE_COUNT * 2))

/**
 * @brief Offset in the GetHandles payload of the NumberField value handle.
 *
 *  Six bytes of header, then the service handle and the declaration and value handles of
 *  OnOff and BooleanField, then the NumberField declaration handle.
 */
#define MEADOW_F7_NUMBER_FIELD_VALUE_OFFSET (6 + (6 * 2))

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
    //  The payload is the handle count (2 bytes) and the length of the handle list (4 bytes),
    //  followed by the handles themselves, two bytes each, in this order:
    //      - Service handle
    //      - 016e99d6-8a61-11eb-8dcd-0242ac1300ff (OnOff), declaration then value
    //      - 017e99d6-8a61-11eb-8dcd-0242ac1300aa (BooleanField), declaration then value
    //      - 018e99d6-8a61-11eb-8dcd-0242ac1300bb (NumberField), declaration then value
    //      - 019e99d6-8a61-11eb-8dcd-0242ac1300cc (TextField), declaration then value
    //
    const uint16_t handleCount = Encoders::ExtractUInt16(response->Payload);
    const uint32_t handlesLength = Encoders::ExtractUInt32(response->Payload + 2);
    TEST_ASSERT_EQUAL(MEADOW_F7_HANDLE_COUNT, handleCount);
    TEST_ASSERT_EQUAL(MEADOW_F7_HANDLE_COUNT * sizeof(uint16_t), handlesLength);
    const uint16_t numberFieldValueHandle = Encoders::ExtractUInt16(response->Payload + MEADOW_F7_NUMBER_FIELD_VALUE_OFFSET);
    Message::DeleteMessage(response);

    WaitForClientConnection(dispatcher);

    //
    //  The client sees the echoed value as soon as the stack has it, which can be before
    //  the response to the ServerDataSet request reaches the outbound queue.  A prompt
    //  client therefore writes its next value first, so the response and the next write
    //  are accepted in either order rather than one strictly after the other.
    //
    const uint32_t progressInterval = UnitTestConfiguration::SoakProgressInterval();
    uint32_t heapAtStart = 0;
    uint32_t heapAtEnd = 0;
    int64_t startedAt = 0;
    //
    //  Iteration at which the heap was first measured, zero until then.
    //
    uint32_t baselineIteration = 0;

    uint32_t iteration = 0;
    uint32_t outstandingDataSetResponses = 0;
    bool clientFinished = false;
    bool clientDisconnected = false;
    while (!clientDisconnected)
    {
        WaitForOutboundMessage(dispatcher, CLIENT_CONNECTION_TIMEOUT);
        TEST_ASSERT_GREATER_THAN(0, dispatcher->OutboundMessageQueueLength());
        response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);

        if (response->Function == BluetoothFunction::ServerDataSet)
        {
            ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::CompletedOk);
            ASSERT_NO_PAYLOAD(response);
            TEST_ASSERT_GREATER_THAN_MESSAGE(0, outstandingDataSetResponses, "ServerDataSet response with no request outstanding.");
            outstandingDataSetResponses--;
            Message::DeleteMessage(response);

            //
            //  The heap is measured with every echo complete, so that the start and the end
            //  are taken in the same state: client connected, nothing in flight.  Everything
            //  up to the first such point is the warm-up, so anything allocated once and
            //  kept is not counted as a leak.
            //
            if (outstandingDataSetResponses == 0)
            {
                heapAtEnd = esp_get_free_heap_size();
                if (baselineIteration == 0)
                {
                    baselineIteration = iteration;
                    heapAtStart = heapAtEnd;
                    startedAt = esp_timer_get_time();
                    printf("Warm-up complete after %" PRIu32 " iterations, free heap %" PRIu32 " bytes.\n", baselineIteration, heapAtStart);
                }
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

        //
        //  The value arrives as an event whose memory was allocated by the
        //  BluetoothRequestHandler, so it is released here.
        //
        ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, StatusCodes::CompletedOk);
        TEST_ASSERT_NOT_NULL(response->Payload);
        TEST_ASSERT_GREATER_OR_EQUAL(6 + sizeof(uint32_t), response->PayloadLength);

        const uint16_t handle = Encoders::ExtractUInt16(response->Payload);
        const uint32_t length = Encoders::ExtractUInt32(response->Payload + 2);
        TEST_ASSERT_EQUAL(numberFieldValueHandle, handle);
        TEST_ASSERT_EQUAL(sizeof(uint32_t), length);
        uint32_t value = 0;
        for (uint32_t index = 0; index < length; index++)
        {
            value |= (static_cast<uint32_t>(response->Payload[6 + index]) << (index * 8));
        }
        Message::DeleteMessage(response);

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
        if ((progressInterval > 0) && ((iteration % progressInterval) == 0) && (baselineIteration > 0) && (iteration > baselineIteration))
        {
            const int64_t elapsed = esp_timer_get_time() - startedAt;
            printf("  %" PRIu32 " iterations, free heap %" PRIu32 " bytes, lowest %" PRIu32 ", %" PRId64 " ms per iteration.\n", iteration, esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), (elapsed / 1000) / (iteration - baselineIteration));
        }

        //
        //  Write the value back to the characteristic for the client to read and verify.
        //
        QueueServerDataSet(dispatcher, handle, value);
        outstandingDataSetResponses++;
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, outstandingDataSetResponses, "ServerDataSet requests left without a response.");
    printf("Client completed %" PRIu32 " iterations.\n", iteration);

    //
    //  A run that never got past the warm-up has nothing to compare.
    //
    if ((baselineIteration > 0) && (iteration > baselineIteration))
    {
        const uint32_t measured = iteration - baselineIteration;
        const int64_t elapsed = esp_timer_get_time() - startedAt;
        printf("Measured %" PRIu32 " iterations in %" PRId64 " seconds.\n", measured, elapsed / 1000000);
        printf("Free heap %" PRIu32 " bytes at the start, %" PRIu32 " at the end, lowest %" PRIu32 ".\n", heapAtStart, heapAtEnd, esp_get_minimum_free_heap_size());

        //
        //  The heap is not expected to come back to exactly where it started, because
        //  allocation patterns leave it slightly differently arranged, but a real leak over
        //  this many round trips is far larger than the allowance the configuration gives.
        //
        if (heapAtEnd < heapAtStart)
        {
            const uint32_t allowance = UnitTestConfiguration::SoakHeapAllowance();
            const uint32_t lost = heapAtStart - heapAtEnd;
            printf("Free heap fell by %" PRIu32 " bytes over %" PRIu32 " iterations, against an allowance of %" PRIu32 ".\n", lost, measured, allowance);
            TEST_ASSERT_TRUE_MESSAGE(lost <= allowance, "The free heap fell too far, which suggests a leak in the characteristic write path.");
        }
    }

    StopBluetoothService(dispatcher);

    ResetMessageDispatcher();
}
