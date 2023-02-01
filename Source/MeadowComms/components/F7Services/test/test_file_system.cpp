/*
 *  test_file_system.cpp
 *
 *  Implementation of the test methods for the file system.
 */

#include "sdkconfig.h"

#include <limits.h>
#include "unity.h"

#include "FileSystem.hpp"

TEST_CASE("File System set up", "[FileSystem]")
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);
    // vTaskDelay(500 / portTICK_PERIOD_MS);
    TEST_ASSERT_TRUE(fs->IsMounted() ? 1 : 0);
    delete fs;
}
