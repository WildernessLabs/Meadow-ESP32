/*
 *  TestUtilsMemory.cpp
 *
 *  Tests that storage is released with the function that matches the one that allocated it (issue #163).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "unity.h"

#include "Utils.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Utils][Emulated]"

/**
 * @brief Bytes the free heap may fall by over a run, for bookkeeping that is not a leak.
 */
#define HEAP_ALLOWANCE 512

/**
 * @brief Number of copies made and released by the leak check.
 */
#define COPY_ITERATIONS 200

TEST_CASE("DuplicateString copies a string to storage of its own", CLASS_OF_TESTS)
{
    const char original[] = "MeadowF7 network";
    char *copy = Utils::DuplicateString(original);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(original, copy, "The copy is the original.");
    TEST_ASSERT_EQUAL_STRING(original, copy);

    //
    //  The copy is the caller's: changing it leaves the original alone.
    //
    copy[0] = 'X';
    TEST_ASSERT_EQUAL_CHAR('M', original[0]);

    Utils::DeleteAndNullifyPointer(copy);
    TEST_ASSERT_NULL_MESSAGE(copy, "DeleteAndNullifyPointer did not clear the pointer.");
}

TEST_CASE("DuplicateString copies an empty string and passes nullptr through", CLASS_OF_TESTS)
{
    char *empty = Utils::DuplicateString("");
    TEST_ASSERT_NOT_NULL_MESSAGE(empty, "An empty string was not copied.");
    TEST_ASSERT_EQUAL_UINT32(0, strlen(empty));
    Utils::DeleteAndNullifyPointer(empty);

    TEST_ASSERT_NULL_MESSAGE(Utils::DuplicateString(nullptr), "A nullptr was not passed through.");
}

TEST_CASE("Strings made by DuplicateString are released in full by DeleteAndNullifyPointer", CLASS_OF_TESTS)
{
    const uint32_t heapBefore = esp_get_free_heap_size();
    for (uint32_t iteration = 0; iteration < COPY_ITERATIONS; iteration++)
    {
        char *copy = Utils::DuplicateString("A string long enough to be noticed if it leaked on every pass.");
        TEST_ASSERT_NOT_NULL(copy);
        Utils::DeleteAndNullifyPointer(copy);
    }
    const uint32_t heapAfter = esp_get_free_heap_size();

    const uint32_t fall = (heapBefore > heapAfter) ? (heapBefore - heapAfter) : 0;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(HEAP_ALLOWANCE, fall, "The free heap fell too far, which suggests the copies are not released.");
}
