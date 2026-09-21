/*
 *  TestEncodersUnit.cpp
 *
 *  Implementation of the test methods for the low-level Encoders decode helpers.
 */

#include "sdkconfig.h"

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
