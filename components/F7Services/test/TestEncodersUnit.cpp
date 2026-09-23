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
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&result));
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
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&result));
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
