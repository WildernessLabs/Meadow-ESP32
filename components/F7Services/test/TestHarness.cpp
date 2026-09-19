/**
 * @file TestHarness.cpp
 * @author Mark Stevens
 * @brief Message dispatcher lifetime and message flow methods shared by the tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include "unity.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Encoders.hpp"

#include "TestHarness.hpp"

/**
 * @brief Message dispatcher shared by the tests.
 */
MockMessageDispatcher *messageDispatcher = nullptr;

/**
 * @brief Interval between checks of the outbound queue, in milliseconds.
 */
static const uint32_t RESPONSE_POLL_INTERVAL = 10;

/**
 * @brief Milliseconds allowed for trailing messages when the dispatcher is reset.
 */
static const uint32_t RESET_SETTLING_TIME = 50;

/**
 * @brief Create the shared message dispatcher if it does not already exist.
 *
 * @return Pointer to the shared message dispatcher, nullptr if it cannot be created.
 */
MockMessageDispatcher *SetupMessageDispatcher()
{
    if (messageDispatcher == nullptr)
    {
        messageDispatcher = new MockMessageDispatcher(nullptr);
    }

    return (messageDispatcher);
}

/**
 * @brief Return the shared message dispatcher to a known state between tests.
 *
 *  The dispatcher is deliberately not destroyed.  The request handlers cache it in
 *  RequestHandlerBase::_messageDispatcher, a static that outlives any one test, and the
 *  Bluetooth handler is a singleton whose task and stack callbacks raise events long
 *  after a test has finished with it.  Destroying the dispatcher leaves all of those
 *  references pointing at freed memory.  The firmware runs a single dispatcher for its
 *  whole life, so the tests model that and release only the messages left behind.
 */
void ResetMessageDispatcher()
{
    if (messageDispatcher != nullptr)
    {
        DrainOutboundQueue(messageDispatcher, RESET_SETTLING_TIME);
    }
}

/**
 * @brief Wait for a message to arrive in the outbound queue, or for a timeout.
 *
 * @param dispatcher Dispatcher whose outbound queue is being watched.
 * @param milliseconds Number of milliseconds to wait.
 */
void WaitForResponse(MockMessageDispatcher *dispatcher, uint32_t milliseconds)
{
    uint32_t waited = 0;

    while ((dispatcher->OutboundMessageQueueLength() == 0) && (waited < milliseconds))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        waited += 1000;
    }
}

/**
 * @brief Wait for the response to a given function, discarding anything else.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 * @param interface Interface the response is expected to come from.
 * @param function Function the response is expected to carry.
 * @param timeout Milliseconds to wait for the response.
 *
 * @return Pointer to the response, which the caller owns, or nullptr on a timeout.
 */
Message *WaitForResponseTo(MockMessageDispatcher *dispatcher, const Esp32Interfaces::Esp32Interfaces interface, const uint32_t function, const unsigned timeout)
{
    unsigned waited = 0;

    while (waited < timeout)
    {
        if (dispatcher->OutboundMessageQueueLength() == 0)
        {
            vTaskDelay(RESPONSE_POLL_INTERVAL / portTICK_PERIOD_MS);
            waited += RESPONSE_POLL_INTERVAL;
            continue;
        }

        Message *message = dispatcher->GetOutboundMessage();
        if (message == nullptr)
        {
            continue;
        }

        if ((message->MessageType == MessageTypes::Response) && (message->Interface == interface) && (message->Function == function))
        {
            return (message);
        }

        //
        //  An event, or the response to something else, so it is of no interest here.
        //
        Message::DeleteMessage(message);
    }

    return (nullptr);
}

/**
 * @brief Queue a request and return the response it produced.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param interface Interface the request is for.
 * @param function Function being called.
 * @param payload Encoded request, which the message takes ownership of.
 * @param payloadLength Length of the encoded request.
 * @param timeout Milliseconds to wait for the response.
 *
 * @return Pointer to the response, which the caller owns, or nullptr on a timeout.
 */
Message *SendRequest(MockMessageDispatcher *dispatcher, const Esp32Interfaces::Esp32Interfaces interface, const uint32_t function, uint8_t *payload, const uint32_t payloadLength, const unsigned timeout)
{
    Message *request = Message::CreateOnHeap(MessageTypes::Header, interface, function, StatusCodes::Failure, 0xff, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(request);
    dispatcher->QueueMessageForEsp32(request);

    return (WaitForResponseTo(dispatcher, interface, function, timeout));
}

/**
 * @brief Queue a request and read back the result and errno it produced.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param interface Interface the request is for.
 * @param function Function being called.
 * @param payload Encoded request, which the message takes ownership of.
 * @param payloadLength Length of the encoded request.
 * @param timeout Milliseconds to wait for the response.
 * @param callResult Filled in with the result and errno from the response.
 *
 * @return True if a response arrived, false if the call timed out.
 */
bool SendRequestForResultAndErrno(MockMessageDispatcher *dispatcher, const Esp32Interfaces::Esp32Interfaces interface, const uint32_t function, uint8_t *payload, const uint32_t payloadLength, const unsigned timeout, SocketCallResult *callResult)
{
    Message *response = SendRequest(dispatcher, interface, function, payload, payloadLength, timeout);

    if (response == nullptr)
    {
        return (false);
    }

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_NOT_NULL(response->Payload);

    callResult->Result = Encoders::ExtractInt32(response->Payload);
    callResult->ResponseErrno = Encoders::ExtractInt32(response->Payload + 4);

    Message::DeleteMessage(response);

    return (true);
}

/**
 * @brief Release every message waiting in the outbound queue.
 *
 * @param dispatcher Dispatcher whose outbound queue is to be emptied.
 * @param settlingTime Milliseconds to wait for any further messages after each one found.
 */
void DrainOutboundQueue(MockMessageDispatcher *dispatcher, const uint32_t settlingTime)
{
    while (dispatcher->OutboundMessageQueueLength() > 0)
    {
        Message *message = dispatcher->GetOutboundMessage();
        if (message != nullptr)
        {
            Message::DeleteMessage(message);
        }
        WaitForResponse(dispatcher, settlingTime);
    }
}
