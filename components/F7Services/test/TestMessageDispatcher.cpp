/**
 * @file TestMessageDispatcher.cpp
 * @author Mark Stevens
 * @brief Implementation of the test methods for the MessageDispatcher class.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include "unity.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "MessageDispatcher.hpp"
#include "MockTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "NvsManager.hpp"

#include "TestAssertions.hpp"

/**
 * @brief Class of tests in this file.
 *
 *  These run under QEMU, but they are deliberately not tagged [Emulated].
 *
 *  The MessageDispatcher constructor creates the WiFi, Bluetooth and System request
 *  handlers, and those are singletons: a second instance throws and the board resets,
 *  which is how the design says a second instance should be treated.  Building a
 *  dispatcher therefore claims those handlers for the rest of the boot, so any later
 *  test in the same run that wants one of its own would reset the board.
 *
 *  Running these as a selector of their own, after [Emulated], keeps that to itself.
 *  See run-tests-ci.sh.
 *
 *  For the same reason there is no test here that builds a second dispatcher.  It would
 *  be testing that the board resets, by resetting the board, which ends the run.
 */
#define CLASS_OF_TESTS "[MessageDispatcher]"

/**
 * @brief Message ID used for the requests made by these tests.
 */
#define TEST_MESSAGE_ID 0x42

/**
 * @brief Interface number that no handler is registered for.
 */
#define UNKNOWN_INTERFACE 0x7f

/**
 * @brief Milliseconds to wait for a request to be answered.
 */
#define RESPONSE_TIMEOUT 5000

/**
 * @brief Interval between checks of the transport's outbound queue, in milliseconds.
 */
#define RESPONSE_POLL_INTERVAL 10

/**
 * @brief Transport the dispatcher under test hands its messages to.
 */
static MockTransportProvider *_transport = nullptr;

/**
 * @brief Dispatcher under test.
 *
 * @note One per run, for the reasons given above.  It is never destroyed: the destructor
 *       is empty and the handlers it created have claimed their singletons for the rest
 *       of the boot, so there is nothing a delete could usefully undo.
 */
static MessageDispatcher *_dispatcher = nullptr;

/**
 * @brief Create the dispatcher and its transport if they do not already exist.
 *
 * @return Pointer to the dispatcher the tests should use.
 */
static MessageDispatcher *SetupDispatcher()
{
    if (_dispatcher == nullptr)
    {
        NvsManager::Setup();

        _transport = new MockTransportProvider();
        TEST_ASSERT_NOT_NULL(_transport);

        _dispatcher = new MessageDispatcher(_transport);
        TEST_ASSERT_NOT_NULL(_dispatcher);
    }

    return (_dispatcher);
}

/**
 * @brief Release every message waiting on the transport.
 */
static void DrainTransport()
{
    while (_transport->OutboundMessageQueueLength() > 0)
    {
        Message *message = _transport->GetOutboundMessage();
        if (message != nullptr)
        {
            Message::DeleteMessage(message);
        }
    }
}

/**
 * @brief Wait for a message to reach the transport.
 *
 * @param timeout Milliseconds to wait.
 *
 * @return Pointer to the message, which the caller owns, or nullptr on a timeout.
 */
static Message *WaitForTransportMessage(const uint32_t timeout)
{
    uint32_t waited = 0;

    while (waited < timeout)
    {
        if (_transport->OutboundMessageQueueLength() > 0)
        {
            return (_transport->GetOutboundMessage());
        }
        vTaskDelay(RESPONSE_POLL_INTERVAL / portTICK_PERIOD_MS);
        waited += RESPONSE_POLL_INTERVAL;
    }

    return (nullptr);
}

/**
 * @brief Check that the message IDs the ESP32 grants have the top bit clear.
 *
 *  The top bit says which end of the link granted the ID.  The STM32 sets it in the IDs
 *  it grants and the ESP32 clears it in the IDs it grants, so an ID that arrived with the
 *  bit set can be told apart from one that was issued here.
 */
TEST_CASE("MessageDispatcher grants message IDs with the top bit clear", CLASS_OF_TESTS)
{
    MessageDispatcher *dispatcher = SetupDispatcher();

    for (int index = 0; index < 16; index++)
    {
        uint32_t messageId = dispatcher->GetNextMessageID();
        TEST_ASSERT_EQUAL_MESSAGE(0, messageId & 0x80000000, "A message ID granted by the ESP32 had the top bit set.");
    }
}

/**
 * @brief Check that a new message ID is granted on each request.
 */
TEST_CASE("MessageDispatcher grants a different message ID each time", CLASS_OF_TESTS)
{
    MessageDispatcher *dispatcher = SetupDispatcher();

    uint32_t previous = dispatcher->GetNextMessageID();
    for (int index = 0; index < 16; index++)
    {
        uint32_t messageId = dispatcher->GetNextMessageID();
        TEST_ASSERT_EQUAL_MESSAGE(previous + 1, messageId, "Message IDs are not consecutive.");
        previous = messageId;
    }
}

/**
 * @brief Check that a response handed to the dispatcher reaches the transport.
 */
TEST_CASE("MessageDispatcher passes a response to the transport", CLASS_OF_TESTS)
{
    MessageDispatcher *dispatcher = SetupDispatcher();

    DrainTransport();
    TEST_ASSERT_EQUAL(0, _transport->OutboundMessageQueueLength());

    Message *response = Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::System, SystemFunction::GetConfiguration, StatusCodes::CompletedOk, TEST_MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_TRUE(dispatcher->QueueMessageForStm32(response));
    TEST_ASSERT_EQUAL(1, _transport->OutboundMessageQueueLength());

    Message *queued = _transport->GetOutboundMessage();
    ASSERT_RESPONSE(queued, Esp32Interfaces::System, SystemFunction::GetConfiguration, StatusCodes::CompletedOk);
    ASSERT_MESSAGE_ID(queued, TEST_MESSAGE_ID);

    Message::DeleteMessage(queued);
    TEST_ASSERT_EQUAL(0, _transport->OutboundMessageQueueLength());
}

/**
 * @brief Check that a request for an interface with no handler is refused.
 *
 * @note The dispatcher takes ownership of a message it refuses and releases it, so the
 *       message must not be touched after the call.
 */
TEST_CASE("MessageDispatcher refuses a request for an unknown interface", CLASS_OF_TESTS)
{
    MessageDispatcher *dispatcher = SetupDispatcher();

    DrainTransport();

    Message *request = Message::CreateOnHeap(MessageTypes::Header, UNKNOWN_INTERFACE, SystemFunction::GetConfiguration, StatusCodes::Failure, TEST_MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    TEST_ASSERT_FALSE_MESSAGE(dispatcher->QueueMessageForEsp32(request), "A request for an unknown interface was accepted.");
    //
    //  Nothing answers a request that was never routed anywhere.
    //
    vTaskDelay(500 / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(0, _transport->OutboundMessageQueueLength());
}

/**
 * @brief Check that a system request is routed to the system handler and answered.
 *
 *  This is the whole path the STM32 uses: the dispatcher picks a handler from the
 *  interface, the handler queues and processes the request, and the response comes back
 *  through the dispatcher to the transport.
 */
TEST_CASE("MessageDispatcher routes a system request and returns the response", CLASS_OF_TESTS)
{
    MessageDispatcher *dispatcher = SetupDispatcher();

    DrainTransport();

    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetConfiguration, StatusCodes::Failure, TEST_MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    TEST_ASSERT_TRUE_MESSAGE(dispatcher->QueueMessageForEsp32(request), "The system request was not accepted.");

    Message *response = WaitForTransportMessage(RESPONSE_TIMEOUT);
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "The system request was never answered.");

    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::GetConfiguration, StatusCodes::CompletedOk);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_HAS_PAYLOAD(response);

    Message::DeleteMessage(response);
}
