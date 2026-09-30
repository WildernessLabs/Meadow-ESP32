/**
 * @file TestFileSystemHelpers.cpp
 * @author Mark Stevens
 * @brief File system setup and C stdio methods used by the file system tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <dirent.h>

#include "unity.h"

#include "esp_littlefs.h"

#include "SharedEnums.hpp"

#include "TestFileSystemHelpers.hpp"

/**
 * @brief Create the dispatcher and file system a test needs, and mount the partition.
 *
 * @param context Filled in with the objects that were created.
 * @param formatFileSystem True to format the partition so the test starts with an empty
 *                         file system.
 */
void CreateFileSystemTestContext(FileSystemTestContext *context, const bool formatFileSystem)
{
    context->Dispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(context->Dispatcher);

    context->Fs = new FileSystem(context->Dispatcher);
    TEST_ASSERT_NOT_NULL(context->Fs);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, context->Fs->Setup());

    if (formatFileSystem)
    {
        TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_format(FileSystem::PARTITION_LABEL));
    }
}

/**
 * @brief Release the objects held in a file system test context.
 *
 * @param context Context whose objects are to be released.
 */
void ReleaseFileSystemTestContext(FileSystemTestContext *context)
{
    if (context->Fs != nullptr)
    {
        delete context->Fs;
        context->Fs = nullptr;
    }
    if (context->Dispatcher != nullptr)
    {
        delete context->Dispatcher;
        context->Dispatcher = nullptr;
    }
}

/**
 * @brief Count the number of files in the root directory of the file system.
 *
 * @return Number of files present, -1 if the directory cannot be opened.
 */
int CountFiles()
{
    DIR *dp = opendir(FileSystem::PARTITION_PATH_NAME);
    int result = 0;

    if (dp == NULL)
    {
        result = -1;
    }
    else
    {
        while (readdir(dp) != NULL)
        {
            result++;
        }
        (void) closedir(dp);
    }

    return (result);
}

/**
 * @brief Write a file to the file system using C stdio.
 *
 * @param filename Name of the file to write.
 * @param buffer Pointer to a block of memory holding the data to write.
 * @param length Number of bytes to write to the file.
 *
 * @return Number of bytes written, -1 if the file cannot be opened.
 */
int WriteTestFile(const char *filename, const uint8_t *buffer, int length)
{
    int result = 0;

    std::string fullPath = FileSystem::PARTITION_PATH_NAME;
    fullPath += "/";
    fullPath += filename;

    FILE *file = fopen(fullPath.c_str(), "w");
    if (file != NULL)
    {
        result = fwrite(static_cast<const void *>(buffer), 1, length, file);
        fclose(file);
    }
    else
    {
        result = -1;
    }

    return (result);
}

/**
 * @brief Read a file from the file system using C stdio.
 *
 * @param filename Name of the file to read.
 * @param buffer Pointer to a block of memory to hold the contents of the file.
 * @param maxLength Maximum number of bytes that the buffer can hold.
 *
 * @return Number of bytes read, -1 if the file cannot be opened.
 */
int ReadTestFile(const char *filename, uint8_t *buffer, int maxLength)
{
    int result = 0;

    std::string fullPath = FileSystem::PARTITION_PATH_NAME;
    fullPath += "/";
    fullPath += filename;

    FILE *file = fopen(fullPath.c_str(), "r");
    if (file != NULL)
    {
        result = fread(static_cast<void *>(buffer), 1, maxLength, file);
        fclose(file);
    }
    else
    {
        result = -1;
    }

    return (result);
}
