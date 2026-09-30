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
#include <string.h>
#include <sys/types.h>
#include "unity.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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
 * @brief Function code that no request handler recognises.
 */
#define UNKNOWN_FUNCTION 0x7ffe

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

/**
 * @brief Send two requests with an unknown function code to one interface and check both are answered.
 *
 *  Each must come back as a NotImplemented response with no payload.  The second answer is
 *  what shows the handler's task is still taking requests after the first: the WiFi task
 *  used to wait forever for a ready bit that nothing set for an unknown function.
 *
 * @param dispatcher Dispatcher to route the requests through.
 * @param interface Interface whose request handler is under test.
 */
static void CheckUnknownFunctionIsAnswered(MessageDispatcher *dispatcher, const Esp32Interfaces::Esp32Interfaces interface)
{
    DrainTransport();

    for (uint32_t attempt = 0; attempt < 2; attempt++)
    {
        uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(4));
        TEST_ASSERT_NOT_NULL(payload);
        memset(payload, 0xa5, 4);
        Message *request = Message::CreateOnHeap(MessageTypes::Header, interface, UNKNOWN_FUNCTION, StatusCodes::Failure, TEST_MESSAGE_ID + attempt, payload, 4);
        TEST_ASSERT_NOT_NULL(request);

        TEST_ASSERT_TRUE_MESSAGE(dispatcher->QueueMessageForEsp32(request), "The request was not accepted.");

        Message *response = WaitForTransportMessage(RESPONSE_TIMEOUT);
        TEST_ASSERT_NOT_NULL_MESSAGE(response, attempt == 0 ? "A request with an unknown function was never answered." : "The handler stopped taking requests after an unknown function.");

        ASSERT_RESPONSE(response, interface, UNKNOWN_FUNCTION, StatusCodes::NotImplemented);
        ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID + attempt);
        ASSERT_NO_PAYLOAD(response);

        Message::DeleteMessage(response);
    }
}

/**
 * @brief An unknown WiFi function is answered NotImplemented and the WiFi task keeps running.
 */
TEST_CASE("An unknown WiFi function is answered and does not hang the WiFi task", CLASS_OF_TESTS)
{
    CheckUnknownFunctionIsAnswered(SetupDispatcher(), Esp32Interfaces::WiFi);
}

/**
 * @brief An unknown System function is answered NotImplemented.
 */
TEST_CASE("An unknown System function is answered", CLASS_OF_TESTS)
{
    CheckUnknownFunctionIsAnswered(SetupDispatcher(), Esp32Interfaces::System);
}

/**
 * @brief An unknown Bluetooth function is answered NotImplemented.
 */
TEST_CASE("An unknown Bluetooth function is answered", CLASS_OF_TESTS)
{
    CheckUnknownFunctionIsAnswered(SetupDispatcher(), Esp32Interfaces::BlueTooth);
}

/**
 * @brief Gives the tests below read access to the transport's outbound queue mutex.
 *
 *  It is never instantiated: constructing a transport provider creates a new mutex and
 *  replaces the one the dispatcher's transport is using.
 */
class OutboundQueueMutexProbe : public TransportProviderBase
{
public:
    /**
     * @brief The mutex guarding the outbound message queue.
     *
     * @return The mutex handle.
     */
    static SemaphoreHandle_t Mutex()
    {
        return (_outboundMessageQueueMutex);
    }
};

/**
 * @brief Set when the redirected output has carried the MessageWaiting control message.
 */
static bool _messageWaitingSeen = false;

/**
 * @brief Set when the queue mutex had a holder while MessageWaiting was being written.
 */
static bool _mutexHeldDuringMessageWaiting = false;

/**
 * @brief Write function for the stream standing in for stdout: note whether the queue
 *        mutex is held when the MessageWaiting control message is written, then discard it.
 *
 * @param cookie Unused.
 * @param buffer Bytes being written.
 * @param size Number of bytes being written.
 * @return size, as every byte is accepted.
 */
static ssize_t RecordMessageWaiting(void *cookie, const char *buffer, size_t size)
{
    (void) cookie;
    if (memmem(buffer, size, "+++MW", 5) != NULL)
    {
        _messageWaitingSeen = true;
        _mutexHeldDuringMessageWaiting |= (xSemaphoreGetMutexHolder(OutboundQueueMutexProbe::Mutex()) != nullptr);
    }
    return (static_cast<ssize_t>(size));
}

/**
 * @brief Check that the MessageWaiting control message is written after the outbound queue
 *        mutex is released, not while it is held (issue #271).
 *
 *  The control message is a blocking UART write.  Holding the mutex across it makes every
 *  task queuing a message, and the transport provider task taking one off, wait on the
 *  UART.  stdout is redirected for this task only, to an unbuffered stream whose write
 *  function looks at the mutex at the moment the control message is written.
 */
TEST_CASE("MessageWaiting is sent after the outbound queue mutex is released", CLASS_OF_TESTS)
{
    MessageDispatcher *dispatcher = SetupDispatcher();
    DrainTransport();

    cookie_io_functions_t functions = {};
    functions.write = RecordMessageWaiting;
    FILE *recorder = fopencookie(nullptr, "w", functions);
    TEST_ASSERT_NOT_NULL(recorder);
    setvbuf(recorder, NULL, _IONBF, 0);

    _messageWaitingSeen = false;
    _mutexHeldDuringMessageWaiting = false;
    Message *response = Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::System, SystemFunction::GetConfiguration, StatusCodes::CompletedOk, TEST_MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(response);

    FILE *console = stdout;
    stdout = recorder;
    const bool queued = dispatcher->QueueMessageForStm32(response);
    stdout = console;
    fclose(recorder);

    TEST_ASSERT_TRUE(queued);
    TEST_ASSERT_TRUE_MESSAGE(_messageWaitingSeen, "The MessageWaiting control message was not written.");
    TEST_ASSERT_FALSE_MESSAGE(_mutexHeldDuringMessageWaiting, "MessageWaiting was written while the outbound queue mutex was held.");

    Message *sent = _transport->GetOutboundMessage();
    ASSERT_MESSAGE_ID(sent, TEST_MESSAGE_ID);
    Message::DeleteMessage(sent);
    TEST_ASSERT_EQUAL(0, _transport->OutboundMessageQueueLength());
}
