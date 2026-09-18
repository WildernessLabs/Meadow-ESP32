/**
 * @file TestCoreDump.cpp
 * @author Mark Stevens
 * @brief Implementation of the test methods for the CoreDump class.
 * @version 0.1
 * @date 2025-03-11
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include "unity.h"

#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "CoreDump.hpp"
#include "Utils.hpp"

#include "TestAssertions.hpp"
#include "TestEncoders.hpp"
#include "TestHarness.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[CoreDump]"

/**
 * @brief Message ID used for the requests made by these tests.
 */
#define MESSAGE_ID 0x01

/**
 * @brief Size of the core dump partition in bytes.
 */
#define CORE_DUMP_PARTITION_SIZE 0x10000

/**
 * @brief Get the core dump information.
 *
 * @param dispatcher Message dispatcher.
 * @param coreDump Core dump object.
 *
 * @return Pointer to a CoreDumpInformationResponse object, which the caller owns.
 */
static Esp32Messaging::CoreDumpInformationResponse *GetCoreDumpInformation(MockMessageDispatcher *dispatcher, CoreDump *coreDump)
{
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetCoreDumpInformation, StatusCodes::CompletedOk, MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    coreDump->GetInformation(request);
    WaitForResponse(dispatcher, 2000);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::GetCoreDumpInformation, StatusCodes::CompletedOk);
    ASSERT_MESSAGE_ID(response, MESSAGE_ID);
    ASSERT_HAS_PAYLOAD(response);

    Esp32Messaging::CoreDumpInformationResponse *information = TestEncoders::ExtractCoreDumpInformation(response->Payload);
    Message::DeleteMessage(response);

    return (information);
}

/**
 * @brief Erase the core dump.
 *
 * @param dispatcher Message dispatcher.
 * @param coreDump Core dump object.
 */
static void EraseCoreDump(MockMessageDispatcher *dispatcher, CoreDump *coreDump)
{
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::EraseCoreDump, StatusCodes::CompletedOk, MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    coreDump->Erase(request);
    WaitForResponse(dispatcher, 1000);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::EraseCoreDump, StatusCodes::CompletedOk);
    ASSERT_MESSAGE_ID(response, MESSAGE_ID);
    ASSERT_NO_PAYLOAD(response);

    Message::DeleteMessage(response);
}

/**
 * @brief Request a fragment of the core dump and check the response.
 *
 * @param dispatcher Message dispatcher.
 * @param coreDump Core dump object.
 * @param request Message the fragment is requested with, which is reused between calls.
 * @param offset Offset within the core dump from which the fragment should start.
 * @param size Number of bytes wanted.
 *
 * @return Pointer to a CoreDumpFragmentResponse object, which the caller owns.
 */
static Esp32Messaging::CoreDumpFragmentResponse *GetCoreDumpFragment(MockMessageDispatcher *dispatcher, CoreDump *coreDump, Message *request, const uint32_t offset, const uint32_t size)
{
    Esp32Messaging::CoreDumpFragmentRequest fragmentRequest = {};
    fragmentRequest.Offset = offset;
    fragmentRequest.Size = size;

    request->PayloadLength = TestEncoders::EncodedCoreDumpFragmentRequestBufferSize(&fragmentRequest);
    request->Payload = TestEncoders::EncodeCoreDumpFragmentRequest(&fragmentRequest);
    TEST_ASSERT_NOT_NULL(request->Payload);

    coreDump->GetFragment(request);
    WaitForResponse(dispatcher, 1000);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, StatusCodes::CompletedOk);
    ASSERT_MESSAGE_ID(response, MESSAGE_ID);
    ASSERT_HAS_PAYLOAD(response);

    Esp32Messaging::CoreDumpFragmentResponse *fragment = TestEncoders::ExtractCoreDumpFragmentResponse(response->Payload);
    response->DeletePayload();
    TEST_ASSERT_NOT_NULL(fragment);

    return (fragment);
}

/**
 * @brief Unit test for getting core dump information.
 */
TEST_CASE("Get core dump information", CLASS_OF_TESTS)
{
    printf("Test: Get core dump information\n");

    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    CoreDump *coreDump = new CoreDump(dispatcher);
    TEST_ASSERT_NOT_NULL(coreDump);

    Esp32Messaging::CoreDumpInformationResponse *information = GetCoreDumpInformation(dispatcher, coreDump);
    TEST_ASSERT_NOT_NULL(information);
    TEST_ASSERT_EQUAL(CORE_DUMP_PARTITION_SIZE, information->PartitionSize);

    printf("Validate the following core dump information:\n");
    printf("IsValid: %s\n", information->IsValid ? "true" : "false");
    printf("Partition Size: 0x%05" PRIx32 "\n", information->PartitionSize);
    printf("Core Dump Size: 0x%05" PRIx32 "\n", information->CoreDumpSize);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&information));

    delete coreDump;
    ReleaseMessageDispatcher();
}

/**
 * @brief Unit test for getting a core dump fragment.
 */
TEST_CASE("Get core dump fragment", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    CoreDump *coreDump = new CoreDump(dispatcher);
    TEST_ASSERT_NOT_NULL(coreDump);

    //
    //  First make sure we have a core dump.
    //
    Esp32Messaging::CoreDumpInformationResponse *information = GetCoreDumpInformation(dispatcher, coreDump);
    TEST_ASSERT_NOT_NULL(information);
    TEST_ASSERT_NOT_EQUAL(0, information->IsValid);
    TEST_ASSERT_GREATER_THAN(0, information->CoreDumpSize);
    //
    //  Test for NULL Payload handling.
    //
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, StatusCodes::CompletedOk, MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    coreDump->GetFragment(request);
    WaitForResponse(dispatcher, 1000);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, StatusCodes::InvalidRequest);
    ASSERT_MESSAGE_ID(response, MESSAGE_ID);
    ASSERT_NO_PAYLOAD(response);
    //
    //  Get the first 256 bytes of the core dump.
    //
    Esp32Messaging::CoreDumpFragmentResponse *fragment = GetCoreDumpFragment(dispatcher, coreDump, request, 0, 256);
    TEST_ASSERT_EQUAL(256, fragment->Size);
    TEST_ASSERT_NOT_NULL(fragment->Data);
    TestEncoders::FreeCoreDumpFragmentResponse(&fragment);
    //
    //  Next we ask for data from the end of the coredump.  This time we ask for more bytes than are available.
    //  We do this by setting the offset to 100 bytes from the end of the core dump and then ask for 256 bytes.
    //  The system should work out that there are only 100 bytes left and just return the 100 bytes.
    //
    fragment = GetCoreDumpFragment(dispatcher, coreDump, request, information->CoreDumpSize - 100, 256);
    TEST_ASSERT_EQUAL(100, fragment->Size);
    TEST_ASSERT_NOT_NULL(fragment->Data);
    TestEncoders::FreeCoreDumpFragmentResponse(&fragment);

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&information));
    Message::DeleteMessage(request);

    delete coreDump;
    ReleaseMessageDispatcher();
}

/**
 * @brief Unit test for erasing the core dump.
 */
TEST_CASE("Erase core dump", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    CoreDump *coreDump = new CoreDump(dispatcher);
    TEST_ASSERT_NOT_NULL(coreDump);

    EraseCoreDump(dispatcher, coreDump);

    Esp32Messaging::CoreDumpInformationResponse *information = GetCoreDumpInformation(dispatcher, coreDump);
    TEST_ASSERT_NOT_NULL(information);
    TEST_ASSERT_EQUAL(0, information->IsValid);
    TEST_ASSERT_EQUAL(CORE_DUMP_PARTITION_SIZE, information->PartitionSize);
    TEST_ASSERT_EQUAL(0, information->CoreDumpSize);

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&information));

    delete coreDump;
    ReleaseMessageDispatcher();
}

/**
 * @brief Generate a core dump.
 *
 * @param count The number of times to recurse before forcing a core dump.
 */
static void GenerateCoreDump(uint32_t count)
{
    if (count == 0)
    {
        printf("Forcing core dump...\n");
        // cppcheck-suppress nullPointer
        *(reinterpret_cast<uint32_t *>(0)) = 0;
    }
    else
    {
        GenerateCoreDump(count - 1);
    }
}

/**
 * @brief Force an assertion failure to make a core dump.
 */
TEST_CASE("Generate a core dump", CLASS_OF_TESTS)
{
    //
    //  A value of 20 will generate a core dump of about 23 KBytes.
    //
    GenerateCoreDump(20);
}
