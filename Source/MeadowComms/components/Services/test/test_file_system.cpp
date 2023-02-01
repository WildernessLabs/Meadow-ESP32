/*
 *  test_file_system.cpp
 *
 *  Implementation of the test methods for the file system.
 */

#include "sdkconfig.h"

#include <limits.h>
#include "unity.h"
#include "Esp32Messaging.hpp"

#include "FileSystem.hpp"

TEST_CASE("Basic file system", "[FileSystem]")
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->IsMounted() ? 1 : 0, 0);
    delete fs;
}
