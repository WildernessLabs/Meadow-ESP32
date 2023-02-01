/*
 *  test_file_system.cpp
 *
 *  Implementation of the test methods for the file system.
 */

#include "sdkconfig.h"

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "FileSystem.hpp"

//
//  Name of the file to be used as a test file.
//
#define TEST_FILE_NAME          "hello.txt"

//
//  Text to be written into the text file.
//
#define TEST_FILE_CONTENTS      "Hello from ESP32 unit tests\n"

/**
 * @brief Check that we can mount the file system.
 */
TEST_CASE("File System set up", "[FileSystem]")
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    TEST_ASSERT_TRUE(esp_littlefs_mounted(FileSystem::PARTITION_LABEL));
    delete fs;
    TEST_ASSERT_FALSE(esp_littlefs_mounted(FileSystem::PARTITION_LABEL));
}

/**
 * @brief Test formatting the LittleFS file system.
 * 
 *  This test will test formatting the LittleFS file system as follows:
 *      - Format the file system and verify that a single success response is received from
 *        the FileSystem class.
 *      - Write a single file to the newly formatted file system using c stdio methods and 
 *        verify that there is exactly one file on the file system.
 *      - Reformat the file system and verify that no files can be found.
 */
TEST_CASE("Formatting file system", "[FileSystem]")
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    //
    //  Create a message to format the file system and call the format method.
    //
    Message *formatMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemFormat, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(formatMessage);

    fs->Format(formatMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemFormat, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());
    //
    //  Write a single file to the file system.
    //
    string s = FileSystem::PARTITION_PATH_NAME;
    s += "/";
    s += TEST_FILE_NAME;
    FILE *file = fopen(s.c_str(), "w");
    TEST_ASSERT_NOT_NULL(file);
    fprintf(file, TEST_FILE_CONTENTS);
    fclose(file);
    //
    //  Check that we have one file on the file system.
    //
    DIR *dp;
    struct dirent *ep;     
    dp = opendir(FileSystem::PARTITION_PATH_NAME);
    TEST_ASSERT_NOT_NULL(dp);
    int count = 0;
    while ((ep = readdir(dp)) != NULL)
    {
        count++;
        TEST_ASSERT_EQUAL_STRING(TEST_FILE_NAME, ep->d_name);
    }            
    (void) closedir(dp);
    TEST_ASSERT_EQUAL(1, count);
    //
    //  Reformat the file system (reuse the message from above).
    //
    fs->Format(formatMessage);
    //
    //  Verify that the file has now gone.
    //
    dp = opendir(FileSystem::PARTITION_PATH_NAME);
    TEST_ASSERT_NOT_NULL(dp);
    count = 0;
    while ((ep = readdir(dp)) != NULL)
    {
        count++;
    }            
    (void) closedir(dp);
    TEST_ASSERT_EQUAL(0, count);

    delete fs;
}

/**
 * @brief Test case for writing a file to the file system.
 * 
 *  Writing a file will be tested by using the messaging system to pass the file
 *  details and contents and then using C stdio to read the file and verify that
 *  the correct contents have been written to the file system.
 * 
 *  The file system will then be unmounted and remounted and the files contents
 *  checked once more using C stdio.
 */
TEST_CASE("Write file to file system", "[FileSystem]")
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);
    //
    //  Start with an empty file system.
    //
    TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_format(FileSystem::PARTITION_LABEL));
    //
    //  Create a message to format the file system and call the format method.
    //
    Message *writeMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemFormat, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(writeMessage);

    fs->WriteFile(writeMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemWriteFile, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());
}