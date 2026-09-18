/**
 * @file TestFileSystemHelpers.hpp
 * @author Mark Stevens
 * @brief File system setup and C stdio methods used by the file system tests.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */
#ifndef _TEST_FILE_SYSTEM_HELPERS_HPP_
#define _TEST_FILE_SYSTEM_HELPERS_HPP_

#include "sdkconfig.h"

#include <stdint.h>

#include "MockMessageDispatcher.hpp"
#include "FileSystem.hpp"

/**
 * @brief The objects a file system test needs in order to drive the FileSystem class.
 */
struct FileSystemTestContext
{
    /**
     * @brief Dispatcher the file system reports through.
     */
    MockMessageDispatcher *Dispatcher;

    /**
     * @brief File system under test.
     */
    FileSystem *Fs;
};

/**
 * @brief Create the dispatcher and file system a test needs, and mount the partition.
 *
 * @param context Filled in with the objects that were created.
 * @param formatFileSystem True to format the partition so the test starts with an empty
 *                         file system.
 */
void CreateFileSystemTestContext(FileSystemTestContext *context, const bool formatFileSystem);

/**
 * @brief Release the objects held in a file system test context.
 *
 * @param context Context whose objects are to be released.  The pointers it holds are
 *                cleared as the objects are destroyed.
 */
void ReleaseFileSystemTestContext(FileSystemTestContext *context);

/**
 * @brief Count the number of files in the root directory of the file system.
 *
 * @return Number of files present, -1 if the directory cannot be opened.
 */
int CountFiles(void);

/**
 * @brief Write a file to the file system using C stdio.
 *
 * @param filename Name of the file to write.
 * @param buffer Pointer to a block of memory holding the data to write.
 * @param length Number of bytes to write to the file.
 *
 * @return Number of bytes written, -1 if the file cannot be opened.
 */
int WriteTestFile(const char *filename, const uint8_t *buffer, int length);

/**
 * @brief Read a file from the file system using C stdio.
 *
 * @param filename Name of the file to read.
 * @param buffer Pointer to a block of memory to hold the contents of the file.
 * @param maxLength Maximum number of bytes that the buffer can hold.
 *
 * @return Number of bytes read, -1 if the file cannot be opened.
 */
int ReadTestFile(const char *filename, uint8_t *buffer, int maxLength);

#endif // _TEST_FILE_SYSTEM_HELPERS_HPP_
