/**
 * @file TestWiFiConfiguration.cpp
 * @author Mark Stevens
 * @brief Implementation of the test methods for the WiFi settings held in NVS.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "WiFiRequestHandler.hpp"
#include "SharedEnums.hpp"
#include "NvsManager.hpp"
#include "SystemRequestHandler.hpp"

/**
 * @brief Class of tests in this file.
 *
 *  These cover the WiFi settings that are held in NVS: the country code and the maximum
 *  retry count.  Both are read by the WiFiRequestHandler constructor, so the tests need a
 *  handler to exist but never need the radio, and they run under QEMU.
 *
 *  They are not tagged [Emulated].  The WiFiRequestHandler is a singleton and a second
 *  instance resets the board by design, so these tests reuse the one the
 *  [MessageDispatcher] tests created rather than building their own.  run-tests-ci.sh
 *  runs this selector after that one for exactly that reason.  Run on their own they
 *  build the handler themselves, which is why the country code is checked for a null
 *  before one is created.
 */
#define CLASS_OF_TESTS "[WiFiConfiguration]"

/**
 * @brief Country code the radio defaults to.
 *
 * @note This code does not scan for or connect to hidden access points.
 */
#define COUNTRY_CODE_DEFAULT "01"

/**
 * @brief Country code that allows hidden access points to be found and joined.
 */
#define COUNTRY_CODE_HIDDEN_APS "GB"

/**
 * @brief A code that is not in the list of valid country codes.
 */
#define COUNTRY_CODE_INVALID "ZZ"

/**
 * @brief Retry count the test sets, chosen to differ from the default of 10.
 */
#define RETRY_COUNT_UNDER_TEST 6

/**
 * @brief Name the maximum retry count is stored under in NVS.
 *
 * @note MAXIMUM_RETRY_COUNT_NAME is private to WiFiRequestHandler, so the name is
 *       repeated here.  A test reading the configuration directly is what shows the
 *       value reached NVS and not just the static it is cached in.
 */
#define RETRY_COUNT_STORAGE_NAME "MaxRetry"

/**
 * @brief Dispatcher used when this file has to build the request handler itself.
 */
static MockMessageDispatcher *_dispatcher = nullptr;

/**
 * @brief Make sure a WiFiRequestHandler exists, without starting the radio.
 *
 *  The country code is read from NVS by the constructor, so a handler has to have been
 *  built before the accessors return anything meaningful.  Setup() is deliberately not
 *  called: it creates a task and an event group that nothing here needs.
 */
static void EnsureRequestHandler()
{
    NvsManager::Setup();

    //
    //  The handler stores the country code in a static that starts as a null pointer and
    //  is filled in by the constructor, so a null says no handler has been built yet.
    //  GetInstance() would say the same thing but is private to the class.
    //
    if (WiFiRequestHandler::GetCountryCode() == nullptr)
    {
        _dispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(_dispatcher);

        WiFiRequestHandler *handler = new WiFiRequestHandler(_dispatcher);
        TEST_ASSERT_NOT_NULL(handler);
    }

    TEST_ASSERT_NOT_NULL_MESSAGE(WiFiRequestHandler::GetCountryCode(), "No WiFi request handler was created.");
}

/**
 * @brief Check that a country code is reported once the handler has been built.
 */
TEST_CASE("Country code is reported after the handler is created", CLASS_OF_TESTS)
{
    EnsureRequestHandler();

    const char *countryCode = WiFiRequestHandler::GetCountryCode();
    TEST_ASSERT_NOT_NULL(countryCode);
    printf("Country code is currently %s\n", countryCode);
    TEST_ASSERT_GREATER_THAN(0, strlen(countryCode));
}

/**
 * @brief Check that the country code can be changed between the two codes that matter.
 *
 *  The two are not interchangeable.  01 is the default and does not scan for or connect
 *  to hidden access points; GB does.  The value is held in NVS and outlives the run, so
 *  whatever the board was set to is put back at the end.
 */
TEST_CASE("Country code can be set to the codes that change hidden AP behaviour", CLASS_OF_TESTS)
{
    EnsureRequestHandler();

    char original[8];
    snprintf(original, sizeof(original), "%s", WiFiRequestHandler::GetCountryCode());
    printf("Country code was %s at the start of this test\n", original);

    //
    //  GB allows hidden access points to be found and joined.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetCountryCode(COUNTRY_CODE_HIDDEN_APS));
    TEST_ASSERT_EQUAL_STRING(COUNTRY_CODE_HIDDEN_APS, WiFiRequestHandler::GetCountryCode());

    //
    //  Setting the code it already holds is accepted and changes nothing.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetCountryCode(COUNTRY_CODE_HIDDEN_APS));
    TEST_ASSERT_EQUAL_STRING(COUNTRY_CODE_HIDDEN_APS, WiFiRequestHandler::GetCountryCode());

    //
    //  01 is the default, and does not reach hidden access points.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetCountryCode(COUNTRY_CODE_DEFAULT));
    TEST_ASSERT_EQUAL_STRING(COUNTRY_CODE_DEFAULT, WiFiRequestHandler::GetCountryCode());

    //
    //  Put back whatever the board was using.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetCountryCode(original));
    TEST_ASSERT_EQUAL_STRING(original, WiFiRequestHandler::GetCountryCode());
}

/**
 * @brief Check that a code that is not in the valid list is refused.
 *
 * @note The list of valid codes is the full set the firmware accepts; only the two codes
 *       above are used by these tests.  This one checks the refusal path with a code that
 *       is in neither.
 */
TEST_CASE("An unknown country code is refused", CLASS_OF_TESTS)
{
    EnsureRequestHandler();

    char original[8];
    snprintf(original, sizeof(original), "%s", WiFiRequestHandler::GetCountryCode());

    TEST_ASSERT_EQUAL(StatusCodes::InvalidCountryCode, WiFiRequestHandler::SetCountryCode(COUNTRY_CODE_INVALID));
    //
    //  A refused code leaves the stored one alone.
    //
    TEST_ASSERT_EQUAL_STRING(original, WiFiRequestHandler::GetCountryCode());
}

/**
 * @brief Check that the maximum retry count survives being set.
 *
 *  The value is held in NVS so that it outlives a restart, in the same way as the country
 *  code.  A value of zero is reported as three, which is the behaviour the getter has
 *  always had, so the test stays clear of zero.
 */
TEST_CASE("Maximum retry count can be set and read back", CLASS_OF_TESTS)
{
    EnsureRequestHandler();

    const uint32_t original = WiFiRequestHandler::GetMaximumRetryCount();
    printf("Maximum retry count was %" PRIu32 " at the start of this test\n", original);
    TEST_ASSERT_GREATER_THAN(0, original);

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetMaximumRetryCount(RETRY_COUNT_UNDER_TEST));
    TEST_ASSERT_EQUAL(RETRY_COUNT_UNDER_TEST, WiFiRequestHandler::GetMaximumRetryCount());

    //
    //  Setting the value it already holds is accepted and changes nothing.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetMaximumRetryCount(RETRY_COUNT_UNDER_TEST));
    TEST_ASSERT_EQUAL(RETRY_COUNT_UNDER_TEST, WiFiRequestHandler::GetMaximumRetryCount());

    //
    //  The value reached NVS rather than only the static it is cached in.  Reading the
    //  configuration directly is what tells those two apart.
    //
    TEST_ASSERT_EQUAL(RETRY_COUNT_UNDER_TEST, SystemRequestHandler::GetConfigurationValue(RETRY_COUNT_STORAGE_NAME, static_cast<uint32_t>(0)));

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, WiFiRequestHandler::SetMaximumRetryCount(original));
    TEST_ASSERT_EQUAL(original, WiFiRequestHandler::GetMaximumRetryCount());
}
