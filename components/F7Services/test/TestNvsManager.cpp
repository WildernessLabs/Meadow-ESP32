/**
 * @file TestNvsManager.cpp
 * @author Wilderness Labs
 * @brief Unit tests for the NvsManager class.
 * @version 0.1
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include "unity.h"

#include "nvs.h"

#include "NvsManager.hpp"
#include "Utils.hpp"

/**
 * @brief Class of tests for NVS manager functionality (shown in the Unity interface).
 */
//
//  [Emulated] marks this as suitable for an unattended run under QEMU.  Tag matching
//  is a substring search, so it still runs when [NvsManager] is selected.
//
#define CLASS_OF_TESTS "[NvsManager][Emulated]"

/**
 * @brief Namespace used exclusively for unit testing to avoid conflicts with production data.
 */
#define TEST_NAMESPACE "nvstest"

/**
 * @brief Erase all keys in the test namespace to ensure a clean slate.
 *
 * Opens the namespace in read/write mode, erases every key, commits the
 * change and closes the handle.  Called both before and after the test case
 * so that no stale data from a previous run can influence results and no
 * test data is left behind on the device.
 */
static void EraseTestNamespace()
{
    nvs_handle_t handle;

    if (nvs_open(TEST_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

/**
 * @brief Unit test covering all NvsManager get and set operations.
 *
 * Verifies round-trip storage for every supported type (string, uint32_t,
 * int32_t, uint8_t, bool), value overwrite behaviour, and correct default-value
 * return when a key does not exist in NVS.
 *
 * @note NVS must already be initialised by app_main before this test runs.
 *       NvsManager::Setup() is called here to set the internal availability flag.
 */
TEST_CASE("NVS Manager operations", CLASS_OF_TESTS)
{
    //
    //  NVS is already initialised by app_main; calling Setup() here sets the
    //  internal _nvsAvailable flag so that all subsequent operations succeed.
    //
    NvsManager::Setup();
    EraseTestNamespace();

    //
    //  ---- String ----
    //
    {
        const char *key = "str_key";
        const char *expected = "hello nvs";
        const char *defaultValue = "default_str";

        TEST_ASSERT_EQUAL(NvsManager::ErrorCodes::Ok, NvsManager::SetValue(TEST_NAMESPACE, key, expected));

        const char *retrieved = NvsManager::GetValue(TEST_NAMESPACE, key, defaultValue);
        TEST_ASSERT_NOT_NULL(retrieved);
        TEST_ASSERT_NOT_EQUAL(defaultValue, retrieved); // heap-allocated copy, not the default pointer
        TEST_ASSERT_EQUAL_STRING(expected, retrieved);
        Utils::DeleteAndNullifyPointer(retrieved);
    }

    //
    //  ---- String overwrite ----
    //
    {
        const char *key = "str_key";
        const char *second = "updated value";
        const char *defaultValue = "default_str";

        TEST_ASSERT_EQUAL(NvsManager::ErrorCodes::Ok, NvsManager::SetValue(TEST_NAMESPACE, key, second));

        const char *retrieved = NvsManager::GetValue(TEST_NAMESPACE, key, defaultValue);
        TEST_ASSERT_NOT_NULL(retrieved);
        TEST_ASSERT_EQUAL_STRING(second, retrieved);
        Utils::DeleteAndNullifyPointer(retrieved);
    }

    //
    //  ---- String default value (key never written) ----
    //
    //  The default comes back as a copy the caller owns, never as the default pointer
    //  itself, so every result can be released the same way (issue #306).  The result used
    //  to be the default pointer on a miss and a heap copy otherwise, and callers could not
    //  tell which they had.
    //
    {
        const char *defaultValue = "fallback";
        const char *retrieved = NvsManager::GetValue(TEST_NAMESPACE, "str_missing", defaultValue);
        TEST_ASSERT_NOT_NULL(retrieved);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(defaultValue, retrieved, "GetValue returned the default pointer rather than a copy.");
        TEST_ASSERT_EQUAL_STRING(defaultValue, retrieved);
        Utils::DeleteAndNullifyPointer(retrieved);
    }

    //
    //  ---- String default of nullptr (key never written) ----
    //
    {
        TEST_ASSERT_NULL(NvsManager::GetValue(TEST_NAMESPACE, "str_missing", nullptr));
    }

    //
    //  ---- uint32_t ----
    //
    {
        const char *key = "u32_key";
        const uint32_t expected = 0xDEADBEEFu;

        TEST_ASSERT_EQUAL(NvsManager::ErrorCodes::Ok, NvsManager::SetValue(TEST_NAMESPACE, key, expected));
        TEST_ASSERT_EQUAL_UINT32(expected, NvsManager::GetValue(TEST_NAMESPACE, key, (uint32_t) 0));
    }

    //
    //  ---- uint32_t overwrite ----
    //
    {
        const char *key = "u32_key";
        const uint32_t second = 99u;

        TEST_ASSERT_EQUAL(NvsManager::ErrorCodes::Ok, NvsManager::SetValue(TEST_NAMESPACE, key, second));
        TEST_ASSERT_EQUAL_UINT32(second, NvsManager::GetValue(TEST_NAMESPACE, key, (uint32_t) 0));
    }

    //
    //  ---- uint32_t default value (key never written) ----
    //
    {
        const uint32_t defaultValue = 42u;
        TEST_ASSERT_EQUAL_UINT32(defaultValue, NvsManager::GetValue(TEST_NAMESPACE, "u32_missing", defaultValue));
    }

    //
    //  ---- int32_t (including negative value) ----
    //
    {
        const char *key = "i32_key";
        const int32_t expected = -12345;

        TEST_ASSERT_EQUAL(NvsManager::ErrorCodes::Ok, NvsManager::SetValue(TEST_NAMESPACE, key, expected));
        TEST_ASSERT_EQUAL_INT32(expected, NvsManager::GetValue(TEST_NAMESPACE, key, (int32_t) 0));
    }

    //
    //  ---- uint8_t ----
    //
    {
        const char *key = "u8_key";
        const uint8_t expected = 0xABu;

        TEST_ASSERT_EQUAL(NvsManager::ErrorCodes::Ok, NvsManager::SetValue(TEST_NAMESPACE, key, expected));
        TEST_ASSERT_EQUAL_UINT8(expected, NvsManager::GetValue(TEST_NAMESPACE, key, (uint8_t) 0));
    }

    //
    //  ---- uint8_t default value (key never written) ----
    //
    {
        const uint8_t defaultValue = 0xFFu;
        TEST_ASSERT_EQUAL_UINT8(defaultValue, NvsManager::GetValue(TEST_NAMESPACE, "u8_missing", defaultValue));
    }

    //
    //  ---- bool (true) ----
    //
    {
        const char *key = "bool_true";

        TEST_ASSERT_EQUAL(NvsManager::ErrorCodes::Ok, NvsManager::SetValue(TEST_NAMESPACE, key, true));
        TEST_ASSERT_TRUE(NvsManager::GetValue(TEST_NAMESPACE, key, false));
    }

    //
    //  ---- bool (false) ----
    //
    {
        const char *key = "bool_false";

        TEST_ASSERT_EQUAL(NvsManager::ErrorCodes::Ok, NvsManager::SetValue(TEST_NAMESPACE, key, false));
        TEST_ASSERT_FALSE(NvsManager::GetValue(TEST_NAMESPACE, key, true));
    }

    EraseTestNamespace();
    printf("[NvsManager] All operations passed.\n");
}
