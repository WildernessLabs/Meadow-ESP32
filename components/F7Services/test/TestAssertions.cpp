/**
 * @file TestAssertions.cpp
 * @author Mark Stevens
 * @brief Assertions for the message headers checked by most of the tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdint.h>

#include "SharedEnums.hpp"

#include "TestAssertions.hpp"

/**
 * @brief Check the type, interface, function and status code of a message.
 *
 * @param message Message to be checked.
 * @param expectedMessageType Message type the message should carry.
 * @param expectedInterface Interface the message should have come from.
 * @param expectedFunction Function the message should relate to.
 * @param expectedStatusCode Status code the message should carry.
 * @param line Line the assertion was made on.
 */
void AssertMessageHeader(const Message * const message, const uint8_t expectedMessageType, const uint8_t expectedInterface, const uint32_t expectedFunction, const uint32_t expectedStatusCode, const UNITY_LINE_TYPE line)
{
    UNITY_TEST_ASSERT_NOT_NULL(message, line, "Expected a message but none was found.");
    UNITY_TEST_ASSERT_EQUAL_INT(expectedMessageType, message->MessageType, line, "Unexpected message type.");
    UNITY_TEST_ASSERT_EQUAL_INT(expectedInterface, message->Interface, line, "Unexpected interface.");
    UNITY_TEST_ASSERT_EQUAL_UINT32(expectedFunction, message->Function, line, "Unexpected function.");
    UNITY_TEST_ASSERT_EQUAL_UINT32(expectedStatusCode, message->StatusCode, line, "Unexpected status code.");
}

/**
 * @brief Check that a message carries no payload.
 *
 * @param message Message to be checked.
 * @param line Line the assertion was made on.
 */
void AssertNoPayload(const Message * const message, const UNITY_LINE_TYPE line)
{
    UNITY_TEST_ASSERT_NOT_NULL(message, line, "Expected a message but none was found.");
    UNITY_TEST_ASSERT_EQUAL_UINT32(0, message->PayloadLength, line, "Expected an empty payload.");
    UNITY_TEST_ASSERT_NULL(message->Payload, line, "Expected no payload.");
}

/**
 * @brief Check that a message carries a payload.
 *
 * @param message Message to be checked.
 * @param line Line the assertion was made on.
 */
void AssertHasPayload(const Message * const message, const UNITY_LINE_TYPE line)
{
    UNITY_TEST_ASSERT_NOT_NULL(message, line, "Expected a message but none was found.");
    UNITY_TEST_ASSERT_GREATER_THAN_INT(0, message->PayloadLength, line, "Expected a payload.");
    UNITY_TEST_ASSERT_NOT_NULL(message->Payload, line, "Expected a payload.");
}

/**
 * @brief Check the message ID a message carries.
 *
 * @param message Message to be checked.
 * @param expectedMessageID Message ID the message should carry.
 * @param line Line the assertion was made on.
 */
void AssertMessageID(const Message * const message, const uint32_t expectedMessageID, const UNITY_LINE_TYPE line)
{
    UNITY_TEST_ASSERT_NOT_NULL(message, line, "Expected a message but none was found.");
    UNITY_TEST_ASSERT_EQUAL_UINT32(expectedMessageID, message->MessageID, line, "Unexpected message ID.");
}

/**
 * @brief Check the number of messages waiting to be sent to the STM32.
 *
 * @param dispatcher Dispatcher whose outbound queue is to be checked.
 * @param expectedLength Number of messages the queue should hold.
 * @param line Line the assertion was made on.
 */
void AssertOutboundQueueLength(MockMessageDispatcher *dispatcher, const int expectedLength, const UNITY_LINE_TYPE line)
{
    UNITY_TEST_ASSERT_NOT_NULL(dispatcher, line, "Expected a message dispatcher but none was found.");
    UNITY_TEST_ASSERT_EQUAL_INT(expectedLength, dispatcher->OutboundMessageQueueLength(), line, "Unexpected number of messages in the outbound queue.");
}
