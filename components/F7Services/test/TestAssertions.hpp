/**
 * @file TestAssertions.hpp
 * @author Mark Stevens
 * @brief Assertions for the message headers checked by most of the tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */
#ifndef _TEST_ASSERTIONS_HPP_
#define _TEST_ASSERTIONS_HPP_

#include "sdkconfig.h"

#include <stdint.h>

#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *   Nearly every test checks the same four or five fields of a message before
 *   it looks at the payload.  The assertions here gather those checks together.
 *
 *   Unity reports a failure against the line number it is given, so each method
 *   takes the line it was called from and passes it on to the UNITY_TEST_ASSERT
 *   macros.  A failure is then reported against the test that failed rather
 *   than against this file.  The macros below supply that line number, so the
 *   tests never pass it themselves.
 *
 * ----------------------------------------------------------------------------
 */

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
void AssertMessageHeader(const Message * const message, const uint8_t expectedMessageType, const uint8_t expectedInterface, const uint32_t expectedFunction, const uint32_t expectedStatusCode, const UNITY_LINE_TYPE line);

/**
 * @brief Check that a message carries no payload.
 *
 * @param message Message to be checked.
 * @param line Line the assertion was made on.
 */
void AssertNoPayload(const Message * const message, const UNITY_LINE_TYPE line);

/**
 * @brief Check that a message carries a payload.
 *
 * @param message Message to be checked.
 * @param line Line the assertion was made on.
 */
void AssertHasPayload(const Message * const message, const UNITY_LINE_TYPE line);

/**
 * @brief Check the message ID a message carries.
 *
 * @param message Message to be checked.
 * @param expectedMessageID Message ID the message should carry.
 * @param line Line the assertion was made on.
 */
void AssertMessageID(const Message * const message, const uint32_t expectedMessageID, const UNITY_LINE_TYPE line);

/**
 * @brief Check the number of messages waiting to be sent to the STM32.
 *
 * @param dispatcher Dispatcher whose outbound queue is to be checked.
 * @param expectedLength Number of messages the queue should hold.
 * @param line Line the assertion was made on.
 */
void AssertOutboundQueueLength(MockMessageDispatcher *dispatcher, const int expectedLength, const UNITY_LINE_TYPE line);

/**
 * @brief Check that a message is a response carrying the given function and status code.
 */
#define ASSERT_RESPONSE(message, expectedInterface, expectedFunction, expectedStatusCode) AssertMessageHeader(message, MessageTypes::Response, expectedInterface, expectedFunction, expectedStatusCode, __LINE__)

/**
 * @brief Check that a message is an event carrying the given function and status code.
 */
#define ASSERT_EVENT(message, expectedInterface, expectedFunction, expectedStatusCode) AssertMessageHeader(message, MessageTypes::Event, expectedInterface, expectedFunction, expectedStatusCode, __LINE__)

/**
 * @brief Check that a message carries no payload.
 */
#define ASSERT_NO_PAYLOAD(message) AssertNoPayload(message, __LINE__)

/**
 * @brief Check that a message carries a payload.
 */
#define ASSERT_HAS_PAYLOAD(message) AssertHasPayload(message, __LINE__)

/**
 * @brief Check the message ID a message carries.
 */
#define ASSERT_MESSAGE_ID(message, expectedMessageID) AssertMessageID(message, expectedMessageID, __LINE__)

/**
 * @brief Check the number of messages waiting to be sent to the STM32.
 */
#define ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, expectedLength) AssertOutboundQueueLength(dispatcher, expectedLength, __LINE__)

#endif // _TEST_ASSERTIONS_HPP_
