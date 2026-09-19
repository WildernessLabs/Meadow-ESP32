/**
 * @file TestBluetoothHelpers.cpp
 * @author Mark Stevens
 * @brief Bluetooth service lifecycle methods used by the Bluetooth tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include "unity.h"

#include "freertos/FreeRTOS.h"

#include "Esp32Messaging.hpp"

#include "TestAssertions.hpp"
#include "TestBluetoothHelpers.hpp"
#include "TestEncoders.hpp"
#include "TestHarness.hpp"

/**
 * @brief Read the next message the Bluetooth service raised.
 *
 * The messages arrive in a known order but are produced by more than one task, so the
 * queue may hold several of them at once or none of them yet.  This waits for the next
 * message to appear and returns it.
 *
 * @param dispatcher Dispatcher whose outbound queue is being read.
 *
 * @return Pointer to the message, which the caller owns.
 */
static Message *NextBluetoothMessage(MockMessageDispatcher *dispatcher)
{
    WaitForResponse(dispatcher, BLUETOOTH_RESPONSE_TIMEOUT);
    TEST_ASSERT_GREATER_THAN(0, dispatcher->OutboundMessageQueueLength());

    Message *message = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(message);

    return (message);
}

/**
 * @brief Make a request of the Bluetooth service and check the messages it raises.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param function Function being requested.
 * @param transitionEvent Event raised as the request starts to be handled.
 * @param completionEvent Event raised once the request has been handled, or
 *                        NO_COMPLETION_EVENT if the request does not raise one.
 * @param expectedStatusCode Status code the response to the request should carry.
 * @param payload Encoded request, nullptr if the request carries no payload.
 * @param payloadLength Length of the encoded request, 0 if the request carries no payload.
 */
void RunBluetoothLifecycleRequest(MockMessageDispatcher *dispatcher, const uint32_t function, const uint32_t transitionEvent, const int32_t completionEvent, const uint32_t expectedStatusCode, uint8_t *payload, const uint32_t payloadLength)
{
    TEST_ASSERT_NOT_NULL(dispatcher);

    //
    //  The request has to be on the heap.  The handler turns the request into the
    //  response to the request, queues that back and the test then releases it, so a
    //  request built on the stack would be freed through a pointer into this function's
    //  stack frame.
    //
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::BlueTooth, function, StatusCodes::Failure, 0, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(request);

    dispatcher->QueueMessageForEsp32(request);

    //
    //  The transition event comes first and carries no payload.
    //
    Message *response = NextBluetoothMessage(dispatcher);
    ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, transitionEvent, StatusCodes::CompletedOk);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);

    //
    //  The completion event follows, for the requests that raise one.
    //
    if (completionEvent != NO_COMPLETION_EVENT)
    {
        response = NextBluetoothMessage(dispatcher);
        ASSERT_EVENT(response, Esp32Interfaces::BlueTooth, static_cast<uint32_t>(completionEvent), StatusCodes::CompletedOk);
        ASSERT_NO_PAYLOAD(response);
        Message::DeleteMessage(response);
    }

    //
    //  Finally the response to the request itself.
    //
    response = NextBluetoothMessage(dispatcher);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, function, expectedStatusCode);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);
}

/**
 * @brief Start the Bluetooth service using the specified configuration.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param configuration Configuration of the service.
 */
void StartBluetoothService(MockMessageDispatcher *dispatcher, const char * const configuration)
{
    Esp32Messaging::BTStackConfig stackConfig = {};
    stackConfig.Config = const_cast<char *>(configuration);

    const uint32_t payloadLength = TestEncoders::EncodedBTStackConfigBufferSize(&stackConfig);
    TEST_ASSERT_GREATER_THAN(0, payloadLength);

    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    TestEncoders::EncodeBTStackConfig(&stackConfig, payload);

    RunBluetoothLifecycleRequest(dispatcher, BluetoothFunction::Start, BluetoothFunction::BluetoothStartingEvent, BluetoothFunction::BluetoothStartedEvent, StatusCodes::CompletedOk, payload, payloadLength);
}

/**
 * @brief Stop the Bluetooth service.
 *
 * @param dispatcher Dispatcher to queue the request on.
 */
void StopBluetoothService(MockMessageDispatcher *dispatcher)
{
    RunBluetoothLifecycleRequest(dispatcher, BluetoothFunction::Stop, BluetoothFunction::BluetoothStoppingEvent, BluetoothFunction::BluetoothStoppedEvent, StatusCodes::CompletedOk, nullptr, 0);
}

/**
 * @brief Queue a request to write a value to a characteristic.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param handle Handle of the characteristic to write to.
 * @param value Value to write to the characteristic.
 */
void QueueServerDataSet(MockMessageDispatcher *dispatcher, const uint16_t handle, const uint32_t value)
{
    uint32_t dataSetValue = value;

    Esp32Messaging::BTServerDataSet dataSet = {};
    dataSet.Handle = handle;
    dataSet.SetData = reinterpret_cast<uint8_t *>(&dataSetValue);
    dataSet.SetDataLength = sizeof(dataSetValue);

    const uint32_t payloadLength = TestEncoders::EncodedBTServerDataSetBufferSize(&dataSet);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(payload);
    TestEncoders::EncodeBTServerDataSet(&dataSet, payload);

    //
    //  The request has to be on the heap, see RunBluetoothLifecycleRequest for why.
    //
    Message *message = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::Failure, 0, payload, payloadLength);
    TEST_ASSERT_NOT_NULL(message);

    dispatcher->QueueMessageForEsp32(message);
}

/**
 * @brief Write a value to a characteristic and check the response to the request.
 *
 * @param dispatcher Dispatcher to queue the request on.
 * @param handle Handle of the characteristic to write to.
 * @param value Value to write to the characteristic.
 * @param timeout Milliseconds to wait for the response.
 */
void WriteServerDataSet(MockMessageDispatcher *dispatcher, const uint16_t handle, const uint32_t value, const uint32_t timeout)
{
    QueueServerDataSet(dispatcher, handle, value);

    //
    //  Wait for the confirmation message that the change has been written back.
    //
    WaitForResponse(dispatcher, timeout);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::ServerDataSet, StatusCodes::CompletedOk);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);
}
