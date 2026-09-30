/**
 * @file TestMessage.cpp
 * @author Mark Stevens
 * @brief Tests for creating and copying Message objects on the heap.
 * @date 2026-09-30
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <string.h>
#include "unity.h"

#include "freertos/FreeRTOS.h"

#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Messaging][Emulated]"

/**
 * @brief Number of payload bytes the tests give a message.
 */
#define TEST_PAYLOAD_LENGTH 16

/**
 * @brief Allocate a payload filled with a recognisable pattern.
 *
 * @return Payload of TEST_PAYLOAD_LENGTH bytes on the FreeRTOS heap.
 */
static uint8_t *CreatePatternPayload()
{
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(TEST_PAYLOAD_LENGTH));
    TEST_ASSERT_NOT_NULL(payload);
    for (uint32_t index = 0; index < TEST_PAYLOAD_LENGTH; index++)
    {
        payload[index] = static_cast<uint8_t>(0xa0 + index);
    }
    return (payload);
}

/**
 * @brief CreateOnHeap stores the fields it is given and starts a new packet at the beginning.
 */
TEST_CASE("CreateOnHeap sets the fields it is given", CLASS_OF_TESTS)
{
    uint8_t *payload = CreatePatternPayload();
    Message *message = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetConfiguration, StatusCodes::Failure, 0x42, payload, TEST_PAYLOAD_LENGTH);
    TEST_ASSERT_NOT_NULL(message);

    TEST_ASSERT_EQUAL_UINT8(MessageTypes::Header, message->MessageType);
    TEST_ASSERT_EQUAL_UINT8(Esp32Interfaces::System, message->Interface);
    TEST_ASSERT_EQUAL_UINT32(SystemFunction::GetConfiguration, message->Function);
    TEST_ASSERT_EQUAL_UINT32(StatusCodes::Failure, message->StatusCode);
    TEST_ASSERT_EQUAL_UINT32(0x42, message->MessageID);
    TEST_ASSERT_EQUAL_UINT16(0, message->PacketOffset);
    TEST_ASSERT_EQUAL_UINT16(0, message->PacketLength);
    TEST_ASSERT_EQUAL_PTR(payload, message->Payload);
    TEST_ASSERT_EQUAL_UINT32(TEST_PAYLOAD_LENGTH, message->PayloadLength);

    Message::DeleteMessage(message);
}

/**
 * @brief CreateCopyOnHeap copies the header, and the payload into a buffer of its own when asked.
 */
TEST_CASE("CreateCopyOnHeap copies the header and, when asked, the payload", CLASS_OF_TESTS)
{
    Message *original = Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::WiFi, WiFiFunction::Send, StatusCodes::CompletedOk, 0x77, CreatePatternPayload(), TEST_PAYLOAD_LENGTH);
    TEST_ASSERT_NOT_NULL(original);

    Message *withPayload = original->CreateCopyOnHeap(true);
    TEST_ASSERT_NOT_NULL(withPayload);
    TEST_ASSERT_EQUAL_UINT8(original->MessageType, withPayload->MessageType);
    TEST_ASSERT_EQUAL_UINT8(original->Interface, withPayload->Interface);
    TEST_ASSERT_EQUAL_UINT32(original->Function, withPayload->Function);
    TEST_ASSERT_EQUAL_UINT32(original->StatusCode, withPayload->StatusCode);
    TEST_ASSERT_EQUAL_UINT32(original->MessageID, withPayload->MessageID);
    TEST_ASSERT_EQUAL_UINT32(TEST_PAYLOAD_LENGTH, withPayload->PayloadLength);
    TEST_ASSERT_NOT_NULL(withPayload->Payload);
    TEST_ASSERT_TRUE_MESSAGE(withPayload->Payload != original->Payload, "The copy shares the original's payload.");
    TEST_ASSERT_EQUAL_HEX8_ARRAY(original->Payload, withPayload->Payload, TEST_PAYLOAD_LENGTH);

    Message *headerOnly = original->CreateCopyOnHeap(false);
    TEST_ASSERT_NOT_NULL(headerOnly);
    TEST_ASSERT_EQUAL_UINT32(original->MessageID, headerOnly->MessageID);
    TEST_ASSERT_NULL(headerOnly->Payload);
    TEST_ASSERT_EQUAL_UINT32(0, headerOnly->PayloadLength);

    Message::DeleteMessage(headerOnly);
    Message::DeleteMessage(withPayload);
    Message::DeleteMessage(original);
}

/**
 * @brief Asked to copy a payload that is not there, CreateCopyOnHeap copies the header only.
 *
 *  Regression check for issue #266.  CreateCopyOnHeap(true) used to memcpy PayloadLength
 *  bytes from Payload without checking it, so a message whose payload had been released
 *  without its length being cleared was copied from address zero.
 */
TEST_CASE("CreateCopyOnHeap copies no payload from a message that has none", CLASS_OF_TESTS)
{
    Message *original = Message::CreateOnHeap(MessageTypes::Response, Esp32Interfaces::WiFi, WiFiFunction::Send, StatusCodes::CompletedOk, 0x78, nullptr, TEST_PAYLOAD_LENGTH);
    TEST_ASSERT_NOT_NULL(original);

    Message *copy = original->CreateCopyOnHeap(true);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_UINT32(original->MessageID, copy->MessageID);
    TEST_ASSERT_NULL(copy->Payload);
    TEST_ASSERT_EQUAL_UINT32(0, copy->PayloadLength);

    Message::DeleteMessage(copy);
    Message::DeleteMessage(original);
}
