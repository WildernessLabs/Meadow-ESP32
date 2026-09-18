/*
 *  TestHelpers.cpp
 *
 *  Implementation of the "test" methods designed to facilitate testing.
 *
 *  These are not really test methods but they are written as test cases to allow them to be called from the test runner.
 */

#include "sdkconfig.h"

#include <stdio.h>
#include "unity.h"

#include "esp_system.h"

#include "test_common.hpp"

#define CLASS_OF_TESTS "[Helpers]"

/**
 * @brief Turn on the generation of heap tracing data.
 */
TEST_CASE("Turn on Heap Tracing Data", CLASS_OF_TESTS)
{
    _generateHeapTraceData = true;
    TEST_PASS();
}

/**
 * @brief Restart the board.
 */
TEST_CASE("Restart the ESP32", CLASS_OF_TESTS)
{
    esp_restart();
    TEST_PASS();
}