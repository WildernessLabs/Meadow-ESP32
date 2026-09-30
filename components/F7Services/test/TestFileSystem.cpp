/*
 *  TestFileSystem.cpp
 *
 *  Implementation of the test methods for the FileSystem class.
 */

#include "sdkconfig.h"

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string>
#include <string.h>
#include "esp_system.h"
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "FileSystem.hpp"
#include "Utils.hpp"

#include "TestAssertions.hpp"
#include "TestEncoders.hpp"
#include "TestFileSystemHelpers.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[FileSystem][Emulated]"

/**
 * @brief Class of tests that take a long time to execute.
 */
#define SLOW_TESTS "[FileSystem][Slow]"

/**
 * @brief Names of the files used by these tests.
 */
#define TEST_FILE_NAME "hello.txt"
#define TEST_FILE_NAME2 "test.txt"

/**
 * @brief Contents of the files used by these tests.
 */
#define TEST_FILE_CONTENTS "Hello from ESP32 unit tests\n"
#define TEST_FILE_CONTENTS2 "More text\n"

/**
 * @brief Message ID used for the requests made by these tests.
 */
#define TEST_MESSAGE_ID 25

/**
 * @brief Name of a file that is never written to the file system.
 */
#define TEST_NON_EXISTENT_FILE_NAME "does_not_exist.txt"

/**
 * @brief Size of a LittleFS block in bytes.
 */
#define LITTLEFS_BLOCK_SIZE 4096

/**
 * @brief Create a message carrying an encoded FileNameAndContents object.
 *
 * @param function Function the message should request.
 * @param name Name of the file the request relates to.
 * @param contents Contents of the file, nullptr when the request only names the file.
 * @param contentsLength Number of bytes of content, 0 when the request only names the file.
 *
 * @return Pointer to a message holding the encoded object in its payload.
 */
static Message *CreateFileNameAndContentsMessage(const uint32_t function, const char *name, const uint8_t *contents, const uint32_t contentsLength)
{
    Esp32Messaging::FileNameAndContents fileData;
    memset(&fileData, 0, sizeof(fileData));
    fileData.Name = const_cast<char *>(name);
    fileData.Contents = const_cast<uint8_t *>(contents);
    fileData.ContentsLength = contentsLength;

    uint32_t payloadLength = Encoders::EncodedFileNameAndContentsBufferSize(&fileData);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileNameAndContents(&fileData, encodedPayload);

    Message *message = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, function, StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(message);

    return (message);
}

/**
 * @brief Write one of the files used by these tests using C stdio.
 *
 * @param filename Name of the file to write.
 * @param contents Contents to be written to the file.
 */
static void WriteTestFileContents(const char *filename, const char *contents)
{
    TEST_ASSERT_EQUAL(strlen(contents), WriteTestFile(filename, reinterpret_cast<const uint8_t *>(contents), strlen(contents)));
}

/**
 * @brief Read a file back and check that it holds the expected contents.
 *
 * @param filename Name of the file to read.
 * @param contents Contents the file is expected to hold.
 */
static void CheckTestFileContents(const char *filename, const char *contents)
{
    const size_t length = strlen(contents);

    uint8_t *buffer = static_cast<uint8_t *>(pvPortMalloc(length + 1));
    TEST_ASSERT_NOT_NULL(buffer);
    memset(buffer, 0, length + 1);

    TEST_ASSERT_EQUAL(length, ReadTestFile(filename, buffer, length));
    TEST_ASSERT_EQUAL_MEMORY(contents, buffer, length);

    Utils::DeleteAndNullifyPointer(buffer);
}

/**
 * @brief Check that we can mount the file system.
 */
TEST_CASE("File System set up", CLASS_OF_TESTS)
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, fs->Setup());
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
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, false);
    //
    //  Create a message to format the file system and call the format method.
    //
    Message *formatMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemFormat, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(formatMessage);

    context.Fs->Format(formatMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemFormat, StatusCodes::CompletedOk);
    ASSERT_NO_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);
    //
    //  Write a single file to the file system.
    //
    WriteTestFileContents(TEST_FILE_NAME, TEST_FILE_CONTENTS);
    //
    //  Check that we have one file on the file system.
    //
    TEST_ASSERT_EQUAL(1, CountFiles());
    //
    //  Reformat the file system (reuse the message from above).
    //
    context.Fs->Format(formatMessage);
    //
    //  Verify that the file has now gone.
    //
    TEST_ASSERT_EQUAL(0, CountFiles());

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Test case for writing a file to the file system.
 *
 *  Writing a file will be tested by using the messaging system to pass the file
 *  details and contents and then using C stdio to read the file and verify that
 *  the correct contents have been written to the file system.
 */
TEST_CASE("Write file to file system", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);
    //
    //  Create a message to write a file to the file system.
    //
    Message *writeMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemWriteFile, TEST_FILE_NAME, reinterpret_cast<const uint8_t *>(TEST_FILE_CONTENTS), strlen(TEST_FILE_CONTENTS));

    context.Fs->WriteFile(writeMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::CompletedOk);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_NO_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    CheckTestFileContents(TEST_FILE_NAME, TEST_FILE_CONTENTS);

    Message::DeleteMessage(writeMessage);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Check that WriteFile rejects a request to create a zero-length file.
 *
 *  A zero-length request carries no Contents, which previously fell through the
 *  write path untouched and reported StatusCodes::Failure without telling the
 *  caller why.  WriteFile must recognise ContentsLength == 0 explicitly, report
 *  StatusCodes::InvalidRequest, and leave the file system untouched.
 */
TEST_CASE("WriteFile rejects a zero-length file", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    //
    //  Build a write request naming a file but carrying no contents.
    //
    Message *writeMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemWriteFile, TEST_FILE_NAME, nullptr, 0);

    context.Fs->WriteFile(writeMessage);

    //
    //  Verify exactly one response is queued and that it reports InvalidRequest
    //  with no payload attached.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::InvalidRequest);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_NO_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    //
    //  The rejected request must not have created a file.
    //
    TEST_ASSERT_EQUAL(0, CountFiles());

    Message::DeleteMessage(writeMessage);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Check that WriteFile rejects a request with no file name.
 *
 *  A request with no Name previously reached std::string::operator+=(nullptr), which is
 *  undefined behaviour (ExtractString returns nullptr for an empty name).  WriteFile must
 *  recognise a missing Name explicitly, report failure, and leave the file system
 *  untouched.
 */
TEST_CASE("WriteFile rejects a request with no file name", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    //
    //  Build a write request carrying contents but no file name.
    //
    Message *writeMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemWriteFile, nullptr, reinterpret_cast<const uint8_t *>(TEST_FILE_CONTENTS), strlen(TEST_FILE_CONTENTS));

    context.Fs->WriteFile(writeMessage);

    //
    //  Verify exactly one response is queued and that it reports failure with no payload
    //  attached.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::Failure);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_NO_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    //
    //  The rejected request must not have created a file.
    //
    TEST_ASSERT_EQUAL(0, CountFiles());

    Message::DeleteMessage(writeMessage);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Check that WriteFile rejects a name that attempts to traverse out of the
 *        littlefs mount point.
 *
 *  Regression check for issue #227.  IsValidFileName rejects every name containing a
 *  path separator, so this also covers the general "no separator is ever permitted"
 *  guarantee, not just the ".." case.
 */
TEST_CASE("WriteFile rejects a path traversal attempt", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    Message *writeMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemWriteFile, "../escaped.txt", reinterpret_cast<const uint8_t *>(TEST_FILE_CONTENTS), strlen(TEST_FILE_CONTENTS));

    context.Fs->WriteFile(writeMessage);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::Failure);
    ASSERT_NO_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    //
    //  Nothing was created inside the mount, and (by construction, since no separator was
    //  accepted) nothing could have been created outside it either.
    //
    TEST_ASSERT_EQUAL(0, CountFiles());

    Message::DeleteMessage(writeMessage);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Check that WriteFile rejects a name with an embedded path separator that is
 *        not a ".." traversal, so the check is not just matching the literal "..".
 */
TEST_CASE("WriteFile rejects an embedded path separator", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    Message *writeMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemWriteFile, "sub/escaped.txt", reinterpret_cast<const uint8_t *>(TEST_FILE_CONTENTS), strlen(TEST_FILE_CONTENTS));

    context.Fs->WriteFile(writeMessage);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::Failure);

    TEST_ASSERT_EQUAL(0, CountFiles());

    Message::DeleteMessage(writeMessage);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Check that WriteFile rejects a name too long for the mounted file system to
 *        store, rather than truncating it or passing it on to fopen().
 */
TEST_CASE("WriteFile rejects a file name that exceeds the file system limit", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    //
    //  CONFIG_LITTLEFS_OBJ_NAME_LEN includes the NUL terminator, so a name of exactly
    //  that length (with no terminator counted against it here) is already too long.
    //
    char overlongName[CONFIG_LITTLEFS_OBJ_NAME_LEN + 1];
    memset(overlongName, 'a', sizeof(overlongName) - 1);
    overlongName[sizeof(overlongName) - 1] = 0;

    Message *writeMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemWriteFile, overlongName, reinterpret_cast<const uint8_t *>(TEST_FILE_CONTENTS), strlen(TEST_FILE_CONTENTS));

    context.Fs->WriteFile(writeMessage);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::Failure);

    TEST_ASSERT_EQUAL(0, CountFiles());

    Message::DeleteMessage(writeMessage);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Test case for reading a file from the file system.
 */
TEST_CASE("Reading a file", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    WriteTestFileContents(TEST_FILE_NAME, TEST_FILE_CONTENTS);

    Message *readMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemReadFile, TEST_FILE_NAME, nullptr, 0);

    context.Fs->ReadFile(readMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemReadFile, StatusCodes::CompletedOk);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_HAS_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    Esp32Messaging::FileNameAndContents *fileContents = Encoders::ExtractFileNameAndContents(response->Payload);
    TEST_ASSERT_NOT_NULL(fileContents);
    TEST_ASSERT_GREATER_THAN(0, fileContents->ContentsLength);
    TEST_ASSERT_NOT_NULL(fileContents->Contents);
    TEST_ASSERT_EQUAL_MEMORY_ARRAY(TEST_FILE_CONTENTS, fileContents->Contents, strlen(TEST_FILE_CONTENTS), 1);

    Encoders::FreeFileNameAndContents(&fileContents);
    Message::DeleteMessage(readMessage);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Read the test file with a request that also carries contents, and check the response.
 *
 * @param context Test context holding the file system under test.
 * @param requestContents Contents to send with the read request.
 * @param requestContentsLength Number of bytes in requestContents.
 */
static void ReadFileWithRequestContents(FileSystemTestContext *context, const uint8_t *requestContents, const uint32_t requestContentsLength)
{
    Message *readMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemReadFile, TEST_FILE_NAME, requestContents, requestContentsLength);
    context->Fs->ReadFile(readMessage);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context->Dispatcher, 1);
    Message *response = context->Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemReadFile, StatusCodes::CompletedOk);

    Esp32Messaging::FileNameAndContents *fileContents = Encoders::ExtractFileNameAndContents(response->Payload);
    TEST_ASSERT_NOT_NULL(fileContents);
    TEST_ASSERT_EQUAL(strlen(TEST_FILE_CONTENTS), fileContents->ContentsLength);
    TEST_ASSERT_EQUAL_MEMORY(TEST_FILE_CONTENTS, fileContents->Contents, strlen(TEST_FILE_CONTENTS));
    Encoders::FreeFileNameAndContents(&fileContents);

    //
    //  The request is recycled as the response, so this releases both.
    //
    Message::DeleteMessage(response);
}

/**
 * @brief A read request that carries contents returns the file and does not leak those contents.
 *
 * @note ExtractFileNameAndContents allocates whatever contents the request declares.  ReadFile
 *       used to overwrite that pointer with the buffer for the file, leaking the request's
 *       contents on every such read.  Ten reads of 1 KB each would lose about 10 KB.
 */
TEST_CASE("ReadFile does not leak contents sent with the read request", CLASS_OF_TESTS)
{
    static constexpr uint32_t REQUEST_CONTENTS_LENGTH = 1024;
    static constexpr uint32_t ITERATIONS = 10;
    static constexpr uint32_t HEAP_ALLOWANCE = 512;

    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);
    WriteTestFileContents(TEST_FILE_NAME, TEST_FILE_CONTENTS);

    uint8_t *requestContents = static_cast<uint8_t *>(pvPortMalloc(REQUEST_CONTENTS_LENGTH));
    TEST_ASSERT_NOT_NULL(requestContents);
    memset(requestContents, 0xa5, REQUEST_CONTENTS_LENGTH);

    //
    //  The first read may leave long-lived allocations behind in the file system and stdio
    //  layers, so the baseline is taken after it.
    //
    ReadFileWithRequestContents(&context, requestContents, REQUEST_CONTENTS_LENGTH);
    const uint32_t heapBefore = esp_get_free_heap_size();

    for (uint32_t iteration = 0; iteration < ITERATIONS; iteration++)
    {
        ReadFileWithRequestContents(&context, requestContents, REQUEST_CONTENTS_LENGTH);
    }

    const uint32_t heapAfter = esp_get_free_heap_size();
    if (heapAfter < heapBefore)
    {
        printf("Free heap fell by %" PRIu32 " bytes over %" PRIu32 " reads, from %" PRIu32 " to %" PRIu32 ".\n", heapBefore - heapAfter, ITERATIONS, heapBefore, heapAfter);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(HEAP_ALLOWANCE, heapBefore - heapAfter, "The free heap fell too far, which suggests ReadFile leaks the request's contents.");
    }

    Utils::DeleteAndNullifyPointer(requestContents);
    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Test getting a list of files on the file system.
 */
TEST_CASE("Get list of files", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    Message *listMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemListFiles, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(listMessage);
    listMessage->MessageID = TEST_MESSAGE_ID;

    context.Fs->ListFiles(listMessage);
    //
    //  Now check we have exactly one response and that it is correct.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemListFiles, StatusCodes::CompletedOk);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_HAS_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    //
    //  We should have no files on the file system.
    //
    Esp32Messaging::FileNameList *files = TestEncoders::ExtractFileNameList(response->Payload);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL(0, files->NumberOfFiles);
    TEST_ASSERT_EQUAL(0, files->FileDetailsLength);
    TEST_ASSERT_NULL(files->FileDetails);
    TestEncoders::FreeFileNameList(&files);
    response->DeletePayload();

    //
    //  Now write two files to the file system and repeat the request reusing the listMessage from above.
    //
    WriteTestFileContents(TEST_FILE_NAME, TEST_FILE_CONTENTS);
    WriteTestFileContents(TEST_FILE_NAME2, TEST_FILE_CONTENTS2);
    listMessage->StatusCode = StatusCodes::Failure;

    uint32_t freeHeapBefore = esp_get_free_heap_size();
    context.Fs->ListFiles(listMessage);
    uint32_t freeHeapAfter = esp_get_free_heap_size();
    TEST_ASSERT_LESS_OR_EQUAL(freeHeapBefore, freeHeapAfter);

    //
    //  We have already tested the message queuing above so we will not repeat those checks, we will
    //  just check that we can retrieve the file information.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    files = TestEncoders::ExtractFileNameList(response->Payload);
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
    TestEncoders::FreeFileNameList(&files);

    Message::DeleteMessage(listMessage);

    ReleaseFileSystemTestContext(&context);
}

/**
 * @brief Check that we can delete a file from the file system.
 */
TEST_CASE("Delete file", CLASS_OF_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    //
    //  Create a message to delete a file from the file system.
    //
    Esp32Messaging::FileDetails details;
    memset(&details, 0, sizeof(details));
    details.Name = const_cast<char *>(TEST_FILE_NAME);
    uint32_t payloadLength = Encoders::EncodedFileDetailsBufferSize(&details);
    uint8_t *encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileDetails(&details, encodedPayload);
    Message *deleteMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemDeleteFile, StatusCodes::Failure, TEST_MESSAGE_ID, encodedPayload, payloadLength);
    TEST_ASSERT_NOT_NULL(deleteMessage);

    context.Fs->DeleteFile(deleteMessage);

    //
    //  Now check we have exactly one response and that it is correct.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemDeleteFile, StatusCodes::FileNotFound);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_NO_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    //
    //  Now create a file and try to delete it.
    //
    WriteTestFileContents(TEST_FILE_NAME, TEST_FILE_CONTENTS);
    TEST_ASSERT_EQUAL(1, CountFiles());

    deleteMessage->PayloadLength = payloadLength;
    encodedPayload = static_cast<uint8_t *>(pvPortMalloc(payloadLength)); // Re-encode as the above call will have freed the memory.
    TEST_ASSERT_NOT_NULL(encodedPayload);
    Encoders::EncodeFileDetails(&details, encodedPayload);
    deleteMessage->Payload = encodedPayload;

    context.Fs->DeleteFile(deleteMessage);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    response = context.Dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);

    TEST_ASSERT_EQUAL(0, CountFiles());

    Message::DeleteMessage(deleteMessage);

    ReleaseFileSystemTestContext(&context);
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
    //  A second Setup() call must fail because the partition is already mounted, and
    //  it must leave the existing mount untouched.  Mount() refuses the duplicate
    //  itself rather than letting LittleFS take an error path that frees the context
    //  belonging to the live mount.
    //
    TEST_ASSERT_EQUAL(StatusCodes::Failure, fs->Setup());
    TEST_ASSERT_TRUE(esp_littlefs_mounted(FileSystem::PARTITION_LABEL));

    //
    //  The file system must still be usable, which is what a freed context would
    //  prevent.  This is the operation that previously panicked on the following test.
    //
    struct stat fileStatus;
    stat(FileSystem::PARTITION_PATH_NAME, &fileStatus);

    delete fs;

    TEST_ASSERT_FALSE(esp_littlefs_mounted(FileSystem::PARTITION_LABEL));
}

/**
 * @brief Check that a request is discarded safely when there is no message dispatcher to
 *        answer it, rather than dereferencing a null pointer.
 *
 *  Regression check for issue #170.  A FileSystem is only ever built without a dispatcher
 *  in a test - production always supplies one through SystemRequestHandler - but
 *  SendResponse still has to cope with it instead of crashing.  ListFiles stands in for
 *  the five methods that recycle their request as its response; they all end by calling
 *  SendResponse.
 */
TEST_CASE("A request is discarded safely when there is no message dispatcher", CLASS_OF_TESTS)
{
    FileSystem *fs = new FileSystem(nullptr);
    TEST_ASSERT_NOT_NULL(fs);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, fs->Setup());

    Message *listMessage = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::FileSystemListFiles, StatusCodes::Failure);
    TEST_ASSERT_NOT_NULL(listMessage);

    //
    //  SendResponse deletes the message itself when there is no dispatcher to queue it
    //  on, so all of it is freed rather than leaked: free heap afterwards must be at
    //  least what it was before the call.
    //
    uint32_t freeHeapBefore = esp_get_free_heap_size();
    fs->ListFiles(listMessage);
    uint32_t freeHeapAfter = esp_get_free_heap_size();
    TEST_ASSERT_GREATER_OR_EQUAL(freeHeapBefore, freeHeapAfter);

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
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

    //
    //  Build a read request for a file that has never been written.
    //
    Message *readMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemReadFile, TEST_NON_EXISTENT_FILE_NAME, nullptr, 0);

    context.Fs->ReadFile(readMessage);

    //
    //  Verify exactly one response is queued and that it reports FileNotFound
    //  with no payload attached.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemReadFile, StatusCodes::FileNotFound);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_NO_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    Message::DeleteMessage(readMessage);

    ReleaseFileSystemTestContext(&context);
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
TEST_CASE("WriteFile returns Failure on a full filesystem", SLOW_TESTS)
{
    FileSystemTestContext context = {};
    CreateFileSystemTestContext(&context, true);

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
    Utils::DeleteAndNullifyPointer(fillBuffer);

    //
    //  Attempt to write a file whose size spans two LittleFS blocks via the
    //  FileSystem API.  After filling, at most one block remains free.  fwrite
    //  can therefore write at most LITTLEFS_BLOCK_SIZE bytes, which is less than
    //  the requested ContentsLength, so WriteFile must return StatusCodes::Failure.
    //
    uint8_t *writeBuffer = static_cast<uint8_t *>(pvPortMalloc(2 * LITTLEFS_BLOCK_SIZE));
    TEST_ASSERT_NOT_NULL(writeBuffer);
    memset(writeBuffer, 0xAA, 2 * LITTLEFS_BLOCK_SIZE);

    Message *writeMessage = CreateFileNameAndContentsMessage(SystemFunction::FileSystemWriteFile, TEST_FILE_NAME, writeBuffer, 2 * LITTLEFS_BLOCK_SIZE);
    Utils::DeleteAndNullifyPointer(writeBuffer);

    context.Fs->WriteFile(writeMessage);

    //
    //  Verify exactly one response is queued and that it reports Failure.
    //
    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 1);

    Message *response = context.Dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::FileSystemWriteFile, StatusCodes::Failure);
    ASSERT_MESSAGE_ID(response, TEST_MESSAGE_ID);
    ASSERT_NO_PAYLOAD(response);

    ASSERT_OUTBOUND_QUEUE_LENGTH(context.Dispatcher, 0);

    Message::DeleteMessage(writeMessage);

    ReleaseFileSystemTestContext(&context);
}
