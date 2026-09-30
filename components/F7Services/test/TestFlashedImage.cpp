/**
 * @file TestFlashedImage.cpp
 * @author Mark Stevens
 * @brief Tests for a LittleFS image built on the host and written to the board.
 * @version 0.1
 * @date 2026-09-19
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include "unity.h"

#include "esp_littlefs.h"

//
//  FileSystem.hpp names IMessageDispatcher without declaring it, so that has to come
//  first.
//
#include "IMessageDispatcher.hpp"
#include "FileSystem.hpp"
#include "SharedEnums.hpp"
#include "Utils.hpp"

#include "TestFileSystemHelpers.hpp"

/**
 * @brief Class of tests in this file.
 *
 *  These read a file system that was built on the host and written to the board, so they
 *  cannot run under emulation: the QEMU image carries nothing in the littlefs partition.
 *
 *  Nothing here formats.  That is the point of them.  FileSystem sets
 *  format_if_mount_failed, so an image the device will not mount is silently replaced
 *  with an empty one rather than being reported, and the only sign of it is the file
 *  having gone.  Every other test in the suite formats before it starts, which would
 *  destroy what these are checking.
 */
#define CLASS_OF_TESTS "[FlashedImage][HardwareOnly]"

/**
 * @brief Name of the file held in the image built by the host.
 *
 * @note The name carries two dots.  Nothing in LittleFS or the VFS layer treats a name as
 *       a stem and an extension, so this is just a name, but it is worth a test rather
 *       than an assumption.
 */
#define FLASHED_FILE_NAME "unittest.config.json"

/**
 * @brief Text the file is expected to contain.
 */
#define FLASHED_FILE_FIRST_LINE "WiFiNetwork"

/**
 * @brief Largest amount of the file the test will read.
 */
#define FLASHED_FILE_BUFFER_SIZE 512

/**
 * @brief Check that the image written to the board mounts and still holds its file.
 *
 *  A reformat on mount is what this is looking for.  It cannot be seen directly, as the
 *  file system reports a successful mount either way, so the test asks for the file
 *  instead: if the partition was reformatted the file has gone, and that is the failure.
 */
TEST_CASE("A flashed file system mounts without being reformatted", CLASS_OF_TESTS)
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);

    TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::CompletedOk, fs->Setup(), "The file system did not mount.");
    TEST_ASSERT_TRUE(esp_littlefs_mounted(FileSystem::PARTITION_LABEL));

    //
    //  The image holds one file.  Nothing on the board has written to it, so anything
    //  else here means the partition was not the one that was flashed.
    //
    const int fileCount = CountFiles();
    printf("Files on the flashed file system: %d\n", fileCount);
    TEST_ASSERT_EQUAL_MESSAGE(1, fileCount, "The flashed image was replaced by an empty file system.");

    delete fs;
}

/**
 * @brief Check that the contents of the flashed file survive the journey.
 */
TEST_CASE("A flashed file can be read back", CLASS_OF_TESTS)
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, fs->Setup());

    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(FLASHED_FILE_BUFFER_SIZE));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, 0, FLASHED_FILE_BUFFER_SIZE);

    const int bytesRead = ReadTestFile(FLASHED_FILE_NAME, buffer, FLASHED_FILE_BUFFER_SIZE - 1);
    printf("Read %d bytes from %s\n", bytesRead, FLASHED_FILE_NAME);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, bytesRead, "The file written by the host was not found.");

    printf("Contents: %s", reinterpret_cast<const char *>(buffer));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(reinterpret_cast<const char *>(buffer), FLASHED_FILE_FIRST_LINE), "The file did not hold the text the host wrote.");

    Utils::DeleteAndNullifyPointer(buffer);

    delete fs;
}
