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
#include <string>
#include <string.h>
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "FileSystem.hpp"
#include "Utils.hpp"

#define CLASS_OF_TESTS "[FileSystem]"

//
//  Name of the files to be used for testing.
//
#define TEST_FILE_NAME "hello.txt"
#define TEST_FILE_NAME2 "test.txt"

//
//  Text to be written into the text files.
//
#define TEST_FILE_CONTENTS "Hello from ESP32 unit tests\n"
#define TEST_FILE_CONTENTS2 "More text\n"

//
//  Message ID to be used for testing.
//
#define TEST_MESSAGE_ID 25

//
//  Constants for file system upgrade testing.
//
#define TEST_UPGRADE_BUFFER_SIZE 1024
#define TEST_UPGRADE_FILE_NAME_AA "upgrade_aa.txt"
#define TEST_UPGRADE_FILE_NAME_55 "upgrade_55.txt"

//
//  Constants for error path testing.
//
#define TEST_NON_EXISTENT_FILE_NAME "does_not_exist.txt"
#define LITTLEFS_BLOCK_SIZE 4096

/**
 * @brief Count the number of files in the root directory of the file system.
 *
 * @return int
 *      Number of files present or -1 if there is a problem.
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
        struct dirent *ep;
        while ((ep = readdir(dp)) != NULL)
        {
            result++;
        }
        (void) closedir(dp);
    }
    return (result);
}

/**
 * @brief Write a test file to the file system.
 *
 * @param filename Name of the file to write.
 * @param buffer Pointer to a block of memory holding the data to write.
 * @param length Number of bytes to write to the file.
 *
 * @return Number of bytes written if successful, -1 otherwise.
 */
int WriteTestFile(const char *filename, const uint8_t *buffer, int length)
{
    int result = 0;

    std::string s = FileSystem::PARTITION_PATH_NAME;
    s += "/";
    s += filename;
    FILE *file = fopen(s.c_str(), "w");
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
 * @brief Read a file from the file system.
 *
 * @param filename Name of the file to read.
 * @param buffer Pointer to a block of memory to hold the contents of the file.
 * @param maxLength Maximum number of bytes that the buffer can hold.
 *
 * @return int Number of bytes read of -1 if there is a problem.
 */
int ReadTestFile(const char *filename, uint8_t *buffer, int maxLength)
{
    int result = 0;

    std::string s = FileSystem::PARTITION_PATH_NAME;
    s += "/";
    s += filename;
    FILE *file = fopen(s.c_str(), "r");
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

/**
 *  Extract the FileNameList object that is encoded in the given buffer.
 *
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling Utils::DeleteAndNullifyPointer(...).
 *
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FileNameList object.
 *
 *  @returns
 *      Pointer to a FileNameList object.
 */
static Esp32Messaging::FileNameList *ExtractFileNameList(uint8_t *buffer)
{
    Esp32Messaging::FileNameList *fileNameList = (Esp32Messaging::FileNameList *) pvPortMalloc(sizeof(Esp32Messaging::FileNameList));

    if (fileNameList == NULL)
    {
        return (nullptr);
    }

    fileNameList->NumberOfFiles = Encoders::ExtractUInt16(buffer);
    buffer += 2;
    fileNameList->FileDetailsLength = Encoders::ExtractUInt32(buffer);
    buffer += 4;
    if (fileNameList->FileDetailsLength > 0)
    {
        fileNameList->FileDetails = (uint8_t *) pvPortMalloc(fileNameList->FileDetailsLength);
        if (fileNameList->FileDetails == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fileNameList));
            return (nullptr);
        }
        memcpy(fileNameList->FileDetails, buffer, fileNameList->FileDetailsLength);
        buffer += fileNameList->FileDetailsLength;
    }
    else
    {
        fileNameList->FileDetails = nullptr;
    }
    return (fileNameList);
}

/**
 * @brief Check that we can mount the file system.
 */
TEST_CASE("File System set up", CLASS_OF_TESTS)
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
TEST_CASE("Formatting file system", CLASS_OF_TESTS)
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
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));
    //
    //  Check that we have one file on the file system.
    //
    TEST_ASSERT_EQUAL(1, CountFiles());
    //
    //  Reformat the file system (reuse the message from above).
    //
    fs->Format(formatMessage);
    //
    //  Verify that the file has now gone.
    //
    TEST_ASSERT_EQUAL(0, CountFiles());

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
TEST_CASE("Write file to file system", CLASS_OF_TESTS)
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
    //  Create a message to write a file to the file system.
    //
    Esp32Messaging::FileNameAndContents fileData;
    memset(&fileData, 0, sizeof(fileData));
    fileData.Name = (char *) TEST_FILE_NAME;
    fileData.Contents = (uint8_t *) TEST_FILE_CONTENTS;
    fileData.ContentsLength = strlen(TEST_FILE_CONTENTS);
    uint32_t payloadLength = Encoders::EncodedFileNameAndContentsBufferSize(&fileData);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileNameAndContents(&fileData, encodedPayload);
    Message *writeMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
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
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    int length = 200;
    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(length));
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), ReadTestFile((char *) TEST_FILE_NAME, buffer, length));
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(TEST_FILE_CONTENTS, buffer, strlen(TEST_FILE_CONTENTS), 1);

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&buffer));
    Message::DeleteMessage(writeMessage);

    delete fs;
}

TEST_CASE("Reading a file", CLASS_OF_TESTS)
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
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));

    Esp32Messaging::FileNameAndContents fileData;
    memset(&fileData, 0, sizeof(fileData));
    fileData.Name = (char *) TEST_FILE_NAME;
    uint32_t payloadLength = Encoders::EncodedFileNameAndContentsBufferSize(&fileData);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileNameAndContents(&fileData, encodedPayload);
    Message *readMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemReadFile, StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(readMessage);

    fs->ReadFile(readMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemReadFile, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);
    TEST_ASSERT_NOT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    Esp32Messaging::FileNameAndContents *fileContents = Encoders::ExtractFileNameAndContents(response->Payload);
    TEST_ASSERT_NOT_NULL(fileContents);
    TEST_ASSERT_GREATER_THAN(0, fileContents->ContentsLength);
    TEST_ASSERT_NOT_NULL(fileContents->Contents);
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(TEST_FILE_CONTENTS, fileContents->Contents, strlen(TEST_FILE_CONTENTS), 1);

    Encoders::FreeFileNameAndContents(&fileContents);
    Message::DeleteMessage(readMessage);

    delete fs;
}

/**
 * @brief Test getting a list of files on the file system.
 */
TEST_CASE("Get list of files", CLASS_OF_TESTS)
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
    Message *listMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemListFiles, StatusCodes::Failure);
    listMessage->MessageID = TEST_MESSAGE_ID;

    fs->ListFiles(listMessage);
    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemListFiles, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);
    TEST_ASSERT_NOT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    //
    //  We should have no files on the file system.
    //
    Esp32Messaging::FileNameList *files = ExtractFileNameList(response->Payload);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL(0, files->NumberOfFiles);
    TEST_ASSERT_EQUAL(0, files->FileDetailsLength);
    TEST_ASSERT_NULL(files->FileDetails);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&files));
    response->DeletePayload();

    //
    //  Now write two files to the file system and repeat the request reusing the listMessage from above.
    //
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS2), WriteTestFile((char *) TEST_FILE_NAME2, (uint8_t *) TEST_FILE_CONTENTS2, strlen(TEST_FILE_CONTENTS2)));
    listMessage->StatusCode = StatusCodes::Failure;

    uint32_t freeHeapBefore = esp_get_free_heap_size();
    fs->ListFiles(listMessage);
    uint32_t freeHeapAfter = esp_get_free_heap_size();
    TEST_ASSERT_LESS_OR_EQUAL(freeHeapBefore, freeHeapAfter);

    //
    //  We have already tested the message queuing above so we will not repeat those checks, we will
    //  just check that we can retrieve the file information.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    files = ExtractFileNameList(response->Payload);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL(2, files->NumberOfFiles);
    TEST_ASSERT_GREATER_THAN(0, files->FileDetailsLength);
    TEST_ASSERT_NOT_NULL(files->FileDetails);
    uint8_t *buffer = files->FileDetails;
    for (int index = 0; index < files->NumberOfFiles; index++)
    {
        Esp32Messaging::FileDetails *file = Encoders::ExtractFileDetails(buffer);
        TEST_ASSERT_NOT_NULL(file);
        buffer += strlen(file->Name) + 1 + sizeof(uint16_t);
        if (strcmp(file->Name, TEST_FILE_NAME) == 0)
        {
            TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), file->Length);
        }
        if (strcmp(file->Name, TEST_FILE_NAME2) == 0)
        {
            TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS2), file->Length);
        }
        Encoders::FreeFileDetails(&file);
    }

    Message::DeleteMessage(listMessage);

    delete fs;
}

/**
 * @brief Check that we can delete a file from the file system.
 */
TEST_CASE("Delete file", CLASS_OF_TESTS)
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
    //  Create a message to delete a file from the file system.
    //
    Esp32Messaging::FileDetails details;
    memset(&details, 0, sizeof(details));
    details.Name = (char *) TEST_FILE_NAME;
    uint32_t payloadLength = Encoders::EncodedFileDetailsBufferSize(&details);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileDetails(&details, encodedPayload);
    Message *deleteMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemDeleteFile, StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(deleteMessage);

    fs->DeleteFile(deleteMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemDeleteFile, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::FileNotFound, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    //
    //  Now create a file and try to delete it.
    //
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));
    TEST_ASSERT_EQUAL(1, CountFiles());

    deleteMessage->PayloadLength = payloadLength;
    encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength)); // Re-encode as the above call will have freed the memory.
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileDetails(&details, encodedPayload);
    deleteMessage->Payload = encodedPayload;

    fs->DeleteFile(deleteMessage);

    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);

    TEST_ASSERT_EQUAL(0, CountFiles());

    Message::DeleteMessage(deleteMessage);

    delete fs;
}

/**
 * @brief Prepare the file system for an upgrade test.
 *
 *  This test will format the file system and then write a number of files to the
 *  file system.
 */
TEST_CASE("Prepare file system upgrade test", CLASS_OF_TESTS)
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
    //  Now put some files on the file system to test the upgrade process.
    //
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), WriteTestFile((char *) TEST_FILE_NAME, (uint8_t *) TEST_FILE_CONTENTS, strlen(TEST_FILE_CONTENTS)));
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS2), WriteTestFile((char *) TEST_FILE_NAME2, (uint8_t *) TEST_FILE_CONTENTS2, strlen(TEST_FILE_CONTENTS2)));

    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(TEST_UPGRADE_BUFFER_SIZE));
    TEST_ASSERT_NOT_NULL(buffer);
    for (int index = 0; index < TEST_UPGRADE_BUFFER_SIZE; index++)
    {
        buffer[index] = 0xaa;
    }
    WriteTestFile(TEST_UPGRADE_FILE_NAME_AA, buffer, TEST_UPGRADE_BUFFER_SIZE);
    for (int index = 0; index < TEST_UPGRADE_BUFFER_SIZE; index++)
    {
        buffer[index] = 0x55;
    }
    WriteTestFile(TEST_UPGRADE_FILE_NAME_55, buffer, TEST_UPGRADE_BUFFER_SIZE);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&buffer));

    //
    //  Now give instructions to the user.
    //
    printf("\n\nFile system for the upgrade test prepared.\n");
    printf("Upgrade the ESP32 firmware to the one containing the new file system library.\n");
    printf("Run the 'Post file system upgrade test' to verify that the file system upgrade works.\n\n\n");

    delete fs;
}

/**
 * @brief Post file system upgrade test.
 *
 *  This test should be run after upgrading the ESP32 firmware to verify that
 *  the file system upgrade process preserved all files correctly. It reads the
 *  files created by the "Prepare file system upgrade test" and verifies their
 *  contents match the expected values.
 */
TEST_CASE("Post file system upgrade test", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(fs->Setup(), StatusCodes::CompletedOk);

    //
    //  Verify the first test file exists and has correct contents.
    //
    uint8_t *buffer1 = static_cast<uint8_t *>(pvPortMalloc(strlen(TEST_FILE_CONTENTS) + 1));
    TEST_ASSERT_NOT_NULL(buffer1);
    memset(buffer1, 0, strlen(TEST_FILE_CONTENTS) + 1);
    int bytesRead = ReadTestFile(TEST_FILE_NAME, buffer1, strlen(TEST_FILE_CONTENTS));
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), bytesRead);
    TEST_ASSERT_EQUAL_MEMORY(TEST_FILE_CONTENTS, buffer1, strlen(TEST_FILE_CONTENTS));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&buffer1));

    //
    //  Verify the second test file exists and has correct contents.
    //
    uint8_t *buffer2 = static_cast<uint8_t *>(pvPortMalloc(strlen(TEST_FILE_CONTENTS2) + 1));
    TEST_ASSERT_NOT_NULL(buffer2);
    memset(buffer2, 0, strlen(TEST_FILE_CONTENTS2) + 1);
    bytesRead = ReadTestFile(TEST_FILE_NAME2, buffer2, strlen(TEST_FILE_CONTENTS2));
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS2), bytesRead);
    TEST_ASSERT_EQUAL_MEMORY(TEST_FILE_CONTENTS2, buffer2, strlen(TEST_FILE_CONTENTS2));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&buffer2));

    //
    //  Verify the 0xAA upgrade test file exists and has correct contents.
    //
    uint8_t *bufferAA = static_cast<uint8_t *>(pvPortMalloc(TEST_UPGRADE_BUFFER_SIZE));
    TEST_ASSERT_NOT_NULL(bufferAA);
    memset(bufferAA, 0, TEST_UPGRADE_BUFFER_SIZE);
    bytesRead = ReadTestFile(TEST_UPGRADE_FILE_NAME_AA, bufferAA, TEST_UPGRADE_BUFFER_SIZE);
    TEST_ASSERT_EQUAL(TEST_UPGRADE_BUFFER_SIZE, bytesRead);
    for (int index = 0; index < TEST_UPGRADE_BUFFER_SIZE; index++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xAA, (uint8_t) bufferAA[index]);
    }
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&bufferAA));

    //
    //  Verify the 0x55 upgrade test file exists and has correct contents.
    //
    uint8_t *buffer55 = static_cast<uint8_t *>(pvPortMalloc(TEST_UPGRADE_BUFFER_SIZE));
    TEST_ASSERT_NOT_NULL(buffer55);
    memset(buffer55, 0, TEST_UPGRADE_BUFFER_SIZE);
    bytesRead = ReadTestFile(TEST_UPGRADE_FILE_NAME_55, buffer55, TEST_UPGRADE_BUFFER_SIZE);
    TEST_ASSERT_EQUAL(TEST_UPGRADE_BUFFER_SIZE, bytesRead);
    for (int index = 0; index < TEST_UPGRADE_BUFFER_SIZE; index++)
    {
        TEST_ASSERT_EQUAL_HEX8(0x55, (uint8_t) buffer55[index]);
    }
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&buffer55));

    //
    //  Verify the correct number of files exist on the file system.
    //
    int fileCount = CountFiles();
    TEST_ASSERT_EQUAL(4, fileCount);

    delete fs;
}

/**
 * @brief Check that Setup() returns Failure when the partition is already mounted.
 *
 *  Calling Setup() a second time on the same FileSystem object triggers a second
 *  call to esp_vfs_littlefs_register with the same partition label.  The LittleFS
 *  VFS layer returns ESP_ERR_INVALID_STATE because the partition is already
 *  registered, so Mount() returns false and Setup() must return
 *  StatusCodes::Failure.  The partition must remain mounted throughout.
 */
TEST_CASE("Mount failure when already mounted", CLASS_OF_TESTS)
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, fs->Setup());
    TEST_ASSERT_TRUE(esp_littlefs_mounted(FileSystem::PARTITION_LABEL));

    //
    //  A second Setup() call must fail because the partition is already registered.
    //  Note: the LittleFS VFS layer releases its internal context as part of the
    //  failed registration, so no assertion is made about the mounted state after
    //  this call.
    //
    TEST_ASSERT_EQUAL(StatusCodes::Failure, fs->Setup());

    delete fs;
}

/**
 * @brief Check that ReadFile returns FileNotFound for a file that does not exist.
 *
 *  The file system is formatted to ensure it is empty, then a read is requested
 *  for a filename that has never been written.  The stat() call inside ReadFile
 *  must fail, causing the response to carry StatusCodes::FileNotFound with no
 *  payload.
 */
TEST_CASE("ReadFile returns FileNotFound for non-existent file", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, fs->Setup());

    //
    //  Start with an empty file system so we can be certain the file does not exist.
    //
    TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_format(FileSystem::PARTITION_LABEL));

    //
    //  Build a read request for a file that has never been written.
    //
    Esp32Messaging::FileNameAndContents fileData;
    memset(&fileData, 0, sizeof(fileData));
    fileData.Name = (char *) TEST_NON_EXISTENT_FILE_NAME;
    uint32_t payloadLength = Encoders::EncodedFileNameAndContentsBufferSize(&fileData);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileNameAndContents(&fileData, encodedPayload);
    Message *readMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemReadFile, StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(readMessage);

    fs->ReadFile(readMessage);

    //
    //  Verify exactly one response is queued and that it reports FileNotFound
    //  with no payload attached.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemReadFile, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::FileNotFound, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    Message::DeleteMessage(readMessage);

    delete fs;
}

/**
 * @brief Check that WriteFile returns Failure when the file system is full.
 *
 *  The file system is filled by writing LITTLEFS_BLOCK_SIZE-byte files via C stdio,
 *  querying esp_littlefs_info after each write to track remaining free space.  Filling
 *  stops once fewer than two blocks remain free, leaving at most one free block.
 *  The WriteFile call then requests 2 * LITTLEFS_BLOCK_SIZE bytes: even if LittleFS
 *  allocates the last free block, fwrite can only write LITTLEFS_BLOCK_SIZE bytes,
 *  which is less than ContentsLength, so WriteFile must return StatusCodes::Failure.
 */
TEST_CASE("WriteFile returns Failure on a full filesystem", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    FileSystem *fs = new FileSystem(messageDispatcher);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, fs->Setup());

    //
    //  Start with an empty file system.
    //
    TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_format(FileSystem::PARTITION_LABEL));

    //
    //  Fill the file system by writing LITTLEFS_BLOCK_SIZE-byte files and querying
    //  esp_littlefs_info after each write.  Stop once fewer than two blocks' worth
    //  of free space remains — at that point LittleFS has no room to allocate a new
    //  data block for the file under test.
    //
    uint8_t *fillBuffer = static_cast<uint8_t *>(pvPortMalloc(LITTLEFS_BLOCK_SIZE));
    TEST_ASSERT_NOT_NULL(fillBuffer);
    memset(fillBuffer, 0xFF, LITTLEFS_BLOCK_SIZE);

    size_t total = 0, used = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_info(FileSystem::PARTITION_LABEL, &total, &used));

    int fillIndex = 0;
    while ((total - used) >= 2 * LITTLEFS_BLOCK_SIZE)
    {
        char fillName[32];
        snprintf(fillName, sizeof(fillName), "fill%d.bin", fillIndex++);
        int written = WriteTestFile(fillName, fillBuffer, LITTLEFS_BLOCK_SIZE);
        if (written != LITTLEFS_BLOCK_SIZE)
        {
            break;
        }
        TEST_ASSERT_EQUAL(ESP_OK, esp_littlefs_info(FileSystem::PARTITION_LABEL, &total, &used));
    }
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fillBuffer));

    //
    //  Attempt to write a file whose size spans two LittleFS blocks via the
    //  FileSystem API.  After filling, at most one block remains free.  fwrite
    //  can therefore write at most LITTLEFS_BLOCK_SIZE bytes, which is less than
    //  the requested ContentsLength, so WriteFile must return StatusCodes::Failure.
    //
    uint8_t *writeBuffer = static_cast<uint8_t *>(pvPortMalloc(2 * LITTLEFS_BLOCK_SIZE));
    TEST_ASSERT_NOT_NULL(writeBuffer);
    memset(writeBuffer, 0xAA, 2 * LITTLEFS_BLOCK_SIZE);

    Esp32Messaging::FileNameAndContents fileData;
    memset(&fileData, 0, sizeof(fileData));
    fileData.Name = (char *) TEST_FILE_NAME;
    fileData.Contents = writeBuffer;
    fileData.ContentsLength = 2 * LITTLEFS_BLOCK_SIZE;
    uint32_t payloadLength = Encoders::EncodedFileNameAndContentsBufferSize(&fileData);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileNameAndContents(&fileData, encodedPayload);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&writeBuffer));
    Message *writeMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::CompletedOk, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(writeMessage);

    fs->WriteFile(writeMessage);

    //
    //  Verify exactly one response is queued and that it reports Failure.
    //
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);

    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::FileSystemWriteFile, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::Failure, response->StatusCode);
    TEST_ASSERT_EQUAL(TEST_MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    TEST_ASSERT_EQUAL(0, messageDispatcher->OutboundMessageQueueLength());

    Message::DeleteMessage(writeMessage);

    delete fs;
}