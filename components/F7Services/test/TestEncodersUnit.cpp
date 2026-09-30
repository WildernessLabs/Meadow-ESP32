/*
 *  TestEncodersUnit.cpp
 *
 *  Implementation of the test methods for the low-level Encoders decode helpers.
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "unity.h"

#include "Encoders.hpp"
#include "Utils.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Encoders][Emulated]"

/**
 * @brief Check that ExtractString decodes a normally terminated string.
 */
TEST_CASE("ExtractString decodes a terminated string", CLASS_OF_TESTS)
{
    const uint8_t * const buffer = reinterpret_cast<const uint8_t *>("hello");

    //
    //  maxLength must cover the terminator itself, so it is strlen("hello") + 1.
    //
    char *result = Encoders::ExtractString(buffer, 6);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello", result);
    Utils::DeleteAndNullifyPointer(result);
}

/**
 * @brief Check that ExtractString returns nullptr for an empty string, matching the
 *        convention EncodeString uses to round-trip a nullptr as a single 0 byte.
 */
TEST_CASE("ExtractString returns nullptr for an empty string", CLASS_OF_TESTS)
{
    const uint8_t buffer[] = {0};

    char *result = Encoders::ExtractString(buffer, sizeof(buffer));
    TEST_ASSERT_NULL(result);
}

/**
 * @brief Check that a terminator sitting inside maxLength is found normally.
 */
TEST_CASE("ExtractString finds a terminator inside maxLength", CLASS_OF_TESTS)
{
    uint8_t buffer[20];
    memset(buffer, 'A', sizeof(buffer));
    buffer[10] = 0;

    char *result = Encoders::ExtractString(buffer, sizeof(buffer));
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(10, strlen(result));
    Utils::DeleteAndNullifyPointer(result);
}

/**
 * @brief Check that ExtractString does not scan past maxLength when the buffer holds no
 *        terminator at all, and reports failure rather than returning an unterminated run
 *        of bytes as a string.
 *
 *  This is the regression check for issue #230: the buffer here is deliberately not
 *  0-terminated anywhere, including past maxLength, so an unbounded scan would walk off
 *  the end of this stack array.
 */
TEST_CASE("ExtractString returns nullptr when no terminator is found within maxLength", CLASS_OF_TESTS)
{
    const uint32_t maxLength = 16;
    uint8_t buffer[maxLength + 4];
    memset(buffer, 'A', sizeof(buffer));

    char *result = Encoders::ExtractString(buffer, maxLength);
    TEST_ASSERT_NULL(result);
}

/**
 * @brief Check that ExtractUInt16 decodes a value whose high byte has its top bit set.
 */
TEST_CASE("ExtractUInt16 decodes a value with the top bit set", CLASS_OF_TESTS)
{
    const uint8_t buffer[] = {0x34, 0xf2};

    TEST_ASSERT_EQUAL_HEX16(0xf234, Encoders::ExtractUInt16(buffer));
}

/**
 * @brief Check that ExtractUInt32 decodes every possible high byte.
 *
 *  Regression check for issue #274: shifting a byte of 0x80 or more left by 24 overflowed
 *  a signed int, which is undefined behaviour, because the byte was promoted to int rather
 *  than uint32_t before the shift.
 */
TEST_CASE("ExtractUInt32 decodes every high byte", CLASS_OF_TESTS)
{
    for (uint32_t highByte = 0; highByte <= 0xff; highByte++)
    {
        const uint32_t expected = (highByte << 24) | 0x00a5b6c7;
        uint8_t buffer[sizeof(uint32_t)];
        Encoders::EncodeUInt32(expected, buffer);
        TEST_ASSERT_EQUAL_HEX32(expected, Encoders::ExtractUInt32(buffer));
    }

    const uint8_t allOnes[] = {0xff, 0xff, 0xff, 0xff};
    TEST_ASSERT_EQUAL_HEX32(0xffffffff, Encoders::ExtractUInt32(allOnes));
}

/**
 * @brief Check that ExtractInt32 round trips negative values and the extremes.
 *
 *  Regression check for issue #274: ExtractInt32 has the same shift as ExtractUInt32, and
 *  it also accumulated into an int32_t while returning a uint32_t.
 */
TEST_CASE("ExtractInt32 round trips negative values and the extremes", CLASS_OF_TESTS)
{
    const int32_t values[] = {0, 1, -1, -2, 0x12345678, -0x12345678, INT32_MAX, INT32_MIN};

    for (const int32_t value: values)
    {
        uint8_t buffer[sizeof(int32_t)];
        Encoders::EncodeInt32(value, buffer);
        TEST_ASSERT_EQUAL_INT32(value, static_cast<int32_t>(Encoders::ExtractInt32(buffer)));
    }

    const uint8_t minusTwo[] = {0xfe, 0xff, 0xff, 0xff};
    TEST_ASSERT_EQUAL_INT32(-2, static_cast<int32_t>(Encoders::ExtractInt32(minusTwo)));
}

/**
 * @brief Write a 32-bit value into a payload being built and move past it.
 *
 * @param position Where to write, advanced by four bytes.
 * @param value Value to write.
 */
static void PutUInt32(uint8_t *&position, const uint32_t value)
{
    Encoders::EncodeUInt32(value, position);
    position += 4;
}

/**
 * @brief Write the fixed part of a sendmsg request: socket, flags, header length and name length.
 *
 * @param position Where to write, advanced past the fields.
 * @param nameLength Name length to declare.
 */
static void PutSendMsgHeader(uint8_t *&position, const uint32_t nameLength)
{
    PutUInt32(position, 54);
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    PutUInt32(position, nameLength);
}

/**
 * @brief Decode a payload and check it is refused as malformed.
 *
 * @param payload Encoded request.
 * @param length Number of bytes in the payload.
 */
static void CheckSendMsgIsMalformed(const uint8_t *payload, const uint32_t length)
{
    bool malformed = false;
    Esp32Messaging::SendMsgRequest *request = Encoders::ExtractSendMsgRequest(payload, length, &malformed);
    TEST_ASSERT_NULL_MESSAGE(request, "A request declaring more than its payload holds was decoded.");
    TEST_ASSERT_TRUE_MESSAGE(malformed, "The refusal was not reported as a malformed request.");
}

/**
 * @brief A well-formed sendmsg request with a name, two iovecs and control data decodes intact.
 */
TEST_CASE("ExtractSendMsgRequest decodes a well-formed request", CLASS_OF_TESTS)
{
    uint8_t payload[128] = {};
    uint8_t *position = payload;
    const uint8_t name[4] = {0x11, 0x22, 0x33, 0x44};
    PutSendMsgHeader(position, sizeof(name));
    memcpy(position, name, sizeof(name));
    position += sizeof(name);
    PutUInt32(position, 2);
    PutUInt32(position, 3);
    PutUInt32(position, 2);
    memcpy(position, "abcde", 5);
    position += 5;
    PutUInt32(position, 2);
    memcpy(position, "zz", 2);
    position += 2;
    PutUInt32(position, 7);

    bool malformed = true;
    Esp32Messaging::SendMsgRequest *request = Encoders::ExtractSendMsgRequest(payload, static_cast<uint32_t>(position - payload), &malformed);
    TEST_ASSERT_NOT_NULL(request);
    TEST_ASSERT_FALSE(malformed);
    TEST_ASSERT_EQUAL_INT32(54, request->SocketHandle);
    TEST_ASSERT_EQUAL_UINT32(sizeof(name), request->NameLength);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(name, request->Name, sizeof(name));
    TEST_ASSERT_EQUAL_UINT32(5, request->BufferLength);
    TEST_ASSERT_EQUAL_MEMORY("abcde", request->Buffer, 5);
    TEST_ASSERT_EQUAL_UINT32(2, request->ControlLength);
    TEST_ASSERT_EQUAL_MEMORY("zz", request->Control, 2);
    TEST_ASSERT_EQUAL_INT32(7, request->MsgFlags);
    Encoders::FreeSendMsgRequest(&request);
}

/**
 * @brief An iovec count the payload cannot hold is refused before the lengths are walked.
 *
 *  Regression check for issue #217: a count of 0xFFFFFFFF used to walk the address space
 *  four bytes at a time until the device faulted.
 */
TEST_CASE("ExtractSendMsgRequest refuses an iovec count the payload cannot hold", CLASS_OF_TESTS)
{
    uint8_t payload[32] = {};
    uint8_t *position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 0xffffffff);
    PutUInt32(position, 4);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));
}

/**
 * @brief iovec lengths whose 32-bit sum wraps to a small value are refused.
 *
 *  0xFFFFFFF8 + 0x10 wraps to 8 in 32 bits, which used to allocate 8 bytes while the
 *  request claimed four gigabytes of data.
 */
TEST_CASE("ExtractSendMsgRequest refuses iovec lengths that wrap when summed", CLASS_OF_TESTS)
{
    uint8_t payload[64] = {};
    uint8_t *position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 2);
    PutUInt32(position, 0xfffffff8);
    PutUInt32(position, 0x10);
    position += 8;
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));
}

/**
 * @brief A name, data or control length longer than the payload is refused.
 */
TEST_CASE("ExtractSendMsgRequest refuses lengths longer than the payload", CLASS_OF_TESTS)
{
    uint8_t payload[64] = {};
    uint8_t *position = payload;
    PutSendMsgHeader(position, 1000);
    PutUInt32(position, 0);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));

    position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 1);
    PutUInt32(position, 1000);
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));

    position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 0);
    PutUInt32(position, 1000);
    PutUInt32(position, 0);
    CheckSendMsgIsMalformed(payload, static_cast<uint32_t>(position - payload));
}

/**
 * @brief A payload that ends part way through a field is refused.
 */
TEST_CASE("ExtractSendMsgRequest refuses a truncated payload", CLASS_OF_TESTS)
{
    uint8_t payload[64] = {};
    uint8_t *position = payload;
    PutSendMsgHeader(position, 0);
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    PutUInt32(position, 0);
    const uint32_t complete = static_cast<uint32_t>(position - payload);

    bool malformed = true;
    Esp32Messaging::SendMsgRequest *request = Encoders::ExtractSendMsgRequest(payload, complete, &malformed);
    TEST_ASSERT_NOT_NULL_MESSAGE(request, "The complete empty request was refused.");
    TEST_ASSERT_FALSE(malformed);
    Encoders::FreeSendMsgRequest(&request);

    for (uint32_t length = 0; length < complete; length++)
    {
        CheckSendMsgIsMalformed(payload, length);
    }
}
