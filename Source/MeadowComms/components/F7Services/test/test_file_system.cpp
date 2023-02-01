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
    string s = "/";
    s += FileSystem::PARTITION_LABEL;
    s+= "/hello.txt";
    FILE *file = fopen(s.c_str(), "w");
    TEST_ASSERT_NOT_NULL(file);
    fprintf(file, "Hello, world\n");
    fclose(file);
    //
    //  Check that we have one file on the file system.
    //
    DIR *dp;
    struct dirent *ep;     
    dp = opendir ("/littlefs");
    TEST_ASSERT_NOT_NULL(dp);
    int count = 0;
    while ((ep = readdir(dp)) != NULL)
    {
        count++;
        TEST_ASSERT_EQUAL_STRING("hello.txt", ep->d_name);
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
    dp = opendir ("/littlefs");
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
