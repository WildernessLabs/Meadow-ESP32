/*
 *  TestLittleFSCompatibility.cpp
 *
 *  Checks that a LittleFS partition written by one firmware release can be read by another.
 *
 *  The ESP-IDF 6 port moved the managed littlefs component from 1.5.5 (LittleFS 2.5) to
 *  1.23.1 (LittleFS 2.11).  The partition holds the device's certificates and keys, and
 *  FileSystem formats a partition it cannot mount, so a release that cannot read what an
 *  earlier or later release wrote loses them without saying so.
 *
 *  These tests are not run by any tag on their own.  They are run by hand, one firmware
 *  writing and another checking, against the same flash: on the board by flashing only
 *  the application, or under QEMU by carrying the littlefs partition from one flash image
 *  to the next.  Each firmware writes its own set of files, named after the ESP-IDF major
 *  version it was built with, so a check after several writers verifies all of them.
 *  The file can be built unchanged against ESP-IDF 5.5 and 6.1 for exactly that reason.
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>

#include "esp_idf_version.h"
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "FileSystem.hpp"
#include "Utils.hpp"

#include "TestFileSystemHelpers.hpp"

/**
 * @brief Class of tests in this file.  Not [Emulated]: the tests only mean something when
 *        another firmware has written to, or will read, the same partition.
 */
#define CLASS_OF_TESTS "[LittleFSCompatibility]"

/**
 * @brief Prefix of every file and directory these tests create.
 */
#define COMPATIBILITY_PREFIX "compat"

/**
 * @brief Directory holding one file of each set, to cover directory metadata as well as the root.
 */
#define COMPATIBILITY_DIRECTORY COMPATIBILITY_PREFIX "-dir"

/**
 * @brief Sizes of the files written by each firmware.
 *
 *  1 and 100 bytes are stored inline in the directory entry, 4095 to 4097 straddle a block,
 *  and 20000 is a list of several blocks.
 */
static const uint32_t FILE_SIZES[] = {1, 100, 4095, 4096, 4097, 20000};

/**
 * @brief Number of entries in FILE_SIZES.
 */
static const uint32_t FILE_SIZE_COUNT = sizeof(FILE_SIZES) / sizeof(FILE_SIZES[0]);

/**
 * @brief Largest entry in FILE_SIZES, the size of the buffers the tests allocate.
 */
static const uint32_t LARGEST_FILE_SIZE = 20000;

/**
 * @brief Work out the byte expected at a position in a file.
 *
 *  The contents are derived from the file name so that every file is different and a file
 *  that has been swapped with another, truncated or partly overwritten is caught.
 *
 * @param name Name of the file, without the directory.
 * @param position Offset of the byte in the file.
 * @return The byte that should be at that offset.
 */
static uint8_t ExpectedByte(const char *name, const uint32_t position)
{
    uint32_t hash = 2166136261u;
    for (const char *character = name; *character != '\0'; character++)
    {
        hash = (hash ^ static_cast<uint8_t>(*character)) * 16777619u;
    }
    return (static_cast<uint8_t>(hash + (position * 31) + (position >> 8)));
}

/**
 * @brief Build the name of one of the files in a set.
 *
 * @param idfMajor ESP-IDF major version of the firmware that writes the set.
 * @param size Size of the file, which is part of its name.
 * @param inDirectory True for the copy kept in COMPATIBILITY_DIRECTORY.
 * @param name Buffer for the name, relative to the partition root.
 * @param nameLength Size of the buffer.
 */
static void CompatibilityFileName(const uint32_t idfMajor, const uint32_t size, const bool inDirectory, char *name, const size_t nameLength)
{
    snprintf(name, nameLength, "%s%s-idf%" PRIu32 "-%" PRIu32 ".bin", inDirectory ? COMPATIBILITY_DIRECTORY "/" : "", COMPATIBILITY_PREFIX, idfMajor, size);
}

/**
 * @brief Fill a buffer with the contents a compatibility file should have.
 *
 * @param name Name of the file relative to the partition root.
 * @param buffer Buffer to fill.
 * @param size Number of bytes to fill.
 */
static void FillExpectedContents(const char *name, uint8_t *buffer, const uint32_t size)
{
    const char *baseName = strrchr(name, '/');
    baseName = (baseName == nullptr) ? name : baseName + 1;
    for (uint32_t position = 0; position < size; position++)
    {
        buffer[position] = ExpectedByte(baseName, position);
    }
}

/**
 * @brief Check one compatibility file against the contents it should have.
 *
 * @param name Name of the file relative to the partition root.
 * @param size Size the file should have.
 * @param expected Buffer of at least LARGEST_FILE_SIZE bytes for the expected contents.
 * @param actual Buffer of at least LARGEST_FILE_SIZE + 1 bytes for the contents read.
 * @return True when the file has exactly the expected size and contents.
 */
static bool CheckCompatibilityFile(const char *name, const uint32_t size, uint8_t *expected, uint8_t *actual)
{
    FillExpectedContents(name, expected, size);
    const int length = ReadTestFile(name, actual, LARGEST_FILE_SIZE + 1);
    if (length != static_cast<int>(size))
    {
        printf("  %s: read %d bytes, expected %" PRIu32 "\n", name, length, size);
        return (false);
    }
    if (memcmp(expected, actual, size) != 0)
    {
        printf("  %s: contents differ\n", name);
        return (false);
    }
    return (true);
}

/**
 * @brief Write this firmware's set of compatibility files, keeping whatever is already there.
 *
 *  The partition is mounted without formatting it, so the files another firmware wrote stay,
 *  and this firmware's writes go into the same directories as theirs.
 */
TEST_CASE("Write the LittleFS compatibility files", CLASS_OF_TESTS)
{
    FileSystemTestContext context;
    CreateFileSystemTestContext(&context, false);

    std::string directory = FileSystem::PARTITION_PATH_NAME;
    directory += "/" COMPATIBILITY_DIRECTORY;
    TEST_ASSERT_TRUE_MESSAGE((mkdir(directory.c_str(), 0775) == 0) || (errno == EEXIST), "Could not create the compatibility directory.");

    uint8_t *contents = new uint8_t[LARGEST_FILE_SIZE];
    TEST_ASSERT_NOT_NULL(contents);
    char name[64];
    for (uint32_t index = 0; index < FILE_SIZE_COUNT; index++)
    {
        for (uint32_t copy = 0; copy < 2; copy++)
        {
            CompatibilityFileName(ESP_IDF_VERSION_MAJOR, FILE_SIZES[index], copy == 1, name, sizeof(name));
            FillExpectedContents(name, contents, FILE_SIZES[index]);
            TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(FILE_SIZES[index]), WriteTestFile(name, contents, FILE_SIZES[index]), name);
        }
    }
    Utils::DeleteAndNullifyPointer(contents);
    printf("LittleFS compatibility: wrote %" PRIu32 " files from ESP-IDF %d.%d.%d\n", FILE_SIZE_COUNT * 2, ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Check every set of compatibility files on the partition, whichever firmware wrote it.
 *
 *  The partition is mounted as the application would mount it, formatting it if the mount
 *  fails.  No compatibility files at all therefore means the partition was not readable and
 *  has been formatted, which is the failure these tests exist to catch.
 */
TEST_CASE("Check the LittleFS compatibility files", CLASS_OF_TESTS)
{
    FileSystemTestContext context;
    CreateFileSystemTestContext(&context, false);

    uint8_t *expected = new uint8_t[LARGEST_FILE_SIZE];
    uint8_t *actual = new uint8_t[LARGEST_FILE_SIZE + 1];
    TEST_ASSERT_NOT_NULL(expected);
    TEST_ASSERT_NOT_NULL(actual);

    uint32_t setsFound = 0;
    uint32_t filesChecked = 0;
    uint32_t failures = 0;
    char name[64];
    for (uint32_t idfMajor = 5; idfMajor <= 6; idfMajor++)
    {
        CompatibilityFileName(idfMajor, FILE_SIZES[0], false, name, sizeof(name));
        std::string firstFile = FileSystem::PARTITION_PATH_NAME;
        firstFile += "/";
        firstFile += name;
        struct stat information;
        if (stat(firstFile.c_str(), &information) != 0)
        {
            printf("LittleFS compatibility: no files from ESP-IDF %" PRIu32 "\n", idfMajor);
            continue;
        }

        setsFound++;
        uint32_t setFailures = 0;
        for (uint32_t index = 0; index < FILE_SIZE_COUNT; index++)
        {
            for (uint32_t copy = 0; copy < 2; copy++)
            {
                CompatibilityFileName(idfMajor, FILE_SIZES[index], copy == 1, name, sizeof(name));
                filesChecked++;
                if (!CheckCompatibilityFile(name, FILE_SIZES[index], expected, actual))
                {
                    setFailures++;
                }
            }
        }
        printf("LittleFS compatibility: files from ESP-IDF %" PRIu32 ": %" PRIu32 " of %" PRIu32 " correct\n", idfMajor, (FILE_SIZE_COUNT * 2) - setFailures, FILE_SIZE_COUNT * 2);
        failures += setFailures;
    }
    printf("LittleFS compatibility: checked by ESP-IDF %d.%d.%d, %" PRIu32 " set(s), %" PRIu32 " file(s), %" PRIu32 " wrong\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH, setsFound, filesChecked, failures);

    Utils::DeleteAndNullifyPointer(expected);
    Utils::DeleteAndNullifyPointer(actual);
    ReleaseFileSystemTestContext(&context);

    TEST_ASSERT_TRUE_MESSAGE(setsFound > 0, "No compatibility files: the partition was not readable and has been formatted, or nothing wrote to it.");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, failures, "Some compatibility files are missing or wrong.");
}
