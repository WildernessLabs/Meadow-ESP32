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

#include <inttypes.h>
#include <stdio.h>
#include "esp_system.h"
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
 *
 *  This is the size partitions-develop.csv gives the partition, and the tests are always
 *  built with the development configuration, so a mismatch here means the board is
 *  running a build made against the application partition table.  That matters to these
 *  tests in particular: the smaller partition cannot hold a dump with the RAM contents
 *  in it, which is the case the retrieval is being proven against.
 */
#define CORE_DUMP_PARTITION_SIZE 0x80000

/**
 * @brief Number of bytes asked for in a single fragment request.
 *
 *  A SPI transaction carries at most 4092 bytes, so the STM32 asks for 4000 at a time
 *  and leaves the rest for the message around them.  The test asks for the same amount
 *  so that it makes the same number of requests, of the same size, as a real retrieval.
 */
#define CORE_DUMP_FRAGMENT_SIZE 4000

/**
 * @brief How far the free heap may fall across a request, in bytes.
 *
 *  A request that gives back everything it took does not always leave the heap reading
 *  exactly as it did beforehand.  The tasks the test is not driving run in between, and
 *  the allocator can split a block differently the second time round.  This allows for
 *  that.  It is not an allowance for a leak: a fragment request holds the fragment twice
 *  over while it encodes it, so anything the fragment path fails to release costs
 *  thousands of bytes on each of the hundred or more requests a full retrieval makes.
 */
#define HEAP_ALLOWANCE 512

/**
 * @brief How often the full retrieval reports its progress, counted in fragments.
 */
#define PROGRESS_INTERVAL 25

/**
 * @brief Check that the free heap has come back to where it was.
 *
 * @param heapBefore Free heap recorded before the work being checked.
 * @param activity What was being done, for the message a failure prints.
 */
static void AssertHeapRecovered(const uint32_t heapBefore, const char *activity)
{
    const uint32_t heapAfter = esp_get_free_heap_size();

    if (heapAfter >= heapBefore)
    {
        return;
    }

    const uint32_t lost = heapBefore - heapAfter;
    if (lost > HEAP_ALLOWANCE)
    {
        printf("Free heap fell by %" PRIu32 " bytes while %s, from %" PRIu32 " to %" PRIu32 ".\n", lost, activity, heapBefore, heapAfter);
        TEST_FAIL_MESSAGE("The free heap fell too far, which suggests a leak in the core dump path.");
    }
}

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
 * @brief Ask for a fragment of the core dump and wait for the response.
 *
 * @note The handler answers in the message it was given, so the response returned here
 *       is the request that was passed in and the caller must not delete it.
 *
 * @param dispatcher Message dispatcher.
 * @param coreDump Core dump object.
 * @param request Message the fragment is requested with, which is reused between calls.
 * @param offset Offset within the core dump from which the fragment should start.
 * @param size Number of bytes wanted.
 *
 * @return Pointer to the response.
 */
static Message *SendCoreDumpFragmentRequest(MockMessageDispatcher *dispatcher, CoreDump *coreDump, Message *request, const uint32_t offset, const uint32_t size)
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
    ASSERT_MESSAGE_ID(response, MESSAGE_ID);

    return (response);
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
    Message *response = SendCoreDumpFragmentRequest(dispatcher, coreDump, request, offset, size);
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, StatusCodes::CompletedOk);
    ASSERT_HAS_PAYLOAD(response);

    Esp32Messaging::CoreDumpFragmentResponse *fragment = TestEncoders::ExtractCoreDumpFragmentResponse(response->Payload);
    response->DeletePayload();
    TEST_ASSERT_NOT_NULL(fragment);

    return (fragment);
}

/**
 * @brief Request a fragment the core dump cannot supply and check how it is refused.
 *
 * @param dispatcher Message dispatcher.
 * @param coreDump Core dump object.
 * @param request Message the fragment is requested with, which is reused between calls.
 * @param offset Offset within the core dump from which the fragment should start.
 * @param size Number of bytes wanted.
 * @param expected Status code the request is expected to be refused with.
 */
static void AssertCoreDumpFragmentRefused(MockMessageDispatcher *dispatcher, CoreDump *coreDump, Message *request, const uint32_t offset, const uint32_t size, const StatusCodes::StatusCodes expected)
{
    Message *response = SendCoreDumpFragmentRequest(dispatcher, coreDump, request, offset, size);
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, expected);
    ASSERT_NO_PAYLOAD(response);
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

    const uint32_t heapBefore = esp_get_free_heap_size();
    Esp32Messaging::CoreDumpInformationResponse *information = GetCoreDumpInformation(dispatcher, coreDump);
    TEST_ASSERT_NOT_NULL(information);
    TEST_ASSERT_EQUAL(CORE_DUMP_PARTITION_SIZE, information->PartitionSize);

    printf("Validate the following core dump information:\n");
    printf("IsValid: %s\n", information->IsValid ? "true" : "false");
    printf("Partition Size: 0x%05" PRIx32 "\n", information->PartitionSize);
    printf("Core Dump Size: 0x%05" PRIx32 "\n", information->CoreDumpSize);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&information));

    AssertHeapRecovered(heapBefore, "getting the core dump information");

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
    uint32_t heapBefore = esp_get_free_heap_size();
    Esp32Messaging::CoreDumpFragmentResponse *fragment = GetCoreDumpFragment(dispatcher, coreDump, request, 0, 256);
    TEST_ASSERT_EQUAL(256, fragment->Size);
    TEST_ASSERT_NOT_NULL(fragment->Data);
    TestEncoders::FreeCoreDumpFragmentResponse(&fragment);
    AssertHeapRecovered(heapBefore, "retrieving a core dump fragment");
    //
    //  Next we ask for the last 100 bytes of the core dump, to show that a fragment
    //  ending exactly on the end of the dump is answered.
    //
    heapBefore = esp_get_free_heap_size();
    fragment = GetCoreDumpFragment(dispatcher, coreDump, request, information->CoreDumpSize - 100, 100);
    TEST_ASSERT_EQUAL(100, fragment->Size);
    TEST_ASSERT_NOT_NULL(fragment->Data);
    TestEncoders::FreeCoreDumpFragmentResponse(&fragment);
    AssertHeapRecovered(heapBefore, "retrieving the last core dump fragment");
    //
    //  A fragment has to lie wholly inside the core dump.  Asking for more than is left
    //  is refused rather than answered with a short fragment, so the caller works out
    //  how much remains and asks for exactly that.
    //
    //  The last of these is the case that used to be dangerous.  An offset at the end of
    //  the dump left the size of the fragment at the size of the whole dump, and the
    //  allocation that followed was large enough to fail and, with the development
    //  configuration, to abort.
    //
    heapBefore = esp_get_free_heap_size();
    AssertCoreDumpFragmentRefused(dispatcher, coreDump, request, information->CoreDumpSize - 100, 256, StatusCodes::InvalidRequest);
    AssertCoreDumpFragmentRefused(dispatcher, coreDump, request, 0, information->CoreDumpSize + 1, StatusCodes::InvalidRequest);
    AssertCoreDumpFragmentRefused(dispatcher, coreDump, request, information->CoreDumpSize, CORE_DUMP_FRAGMENT_SIZE, StatusCodes::InvalidRequest);
    AssertCoreDumpFragmentRefused(dispatcher, coreDump, request, UINT32_MAX, CORE_DUMP_FRAGMENT_SIZE, StatusCodes::InvalidRequest);
    //
    //  A request for nothing at all cannot be answered either.
    //
    AssertCoreDumpFragmentRefused(dispatcher, coreDump, request, 0, 0, StatusCodes::InvalidRequest);
    AssertHeapRecovered(heapBefore, "refusing core dump fragments");

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&information));
    Message::DeleteMessage(request);

    delete coreDump;
    ReleaseMessageDispatcher();
}

/**
 * @brief Unit test for retrieving a whole core dump a fragment at a time.
 *
 *  The two tests above prove a single fragment request.  This one follows the sequence
 *  the STM32 actually uses to lift a dump off the board:
 *
 *      1 - ask for the core dump information;
 *      2 - stop if the dump is invalid or empty;
 *      3 - ask for 4000 bytes, or whatever is left if that is less, and count what came
 *          back;
 *      4 - repeat until the whole dump has been retrieved.
 *
 *  The STM32 assembles the dump, so nothing here keeps the bytes.  What is being proven
 *  is that every fragment of a full sized dump can be asked for and answered, and that
 *  the hundred or more requests it takes leave the heap where they found it.  The last
 *  point is the reason for the test: the retrieval used to end in a core dump of its own
 *  once the partition was large enough to hold the RAM contents, which pointed at
 *  something the fragment path was not giving back.
 *
 * @note This needs a core dump to read, so run "Generate a core dump" first.  It is also
 *       the reason it is not tagged [Emulated]: the tests in that batch must not be
 *       reordered around the restart that generating a dump causes.
 */
TEST_CASE("Retrieve the whole core dump", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    CoreDump *coreDump = new CoreDump(dispatcher);
    TEST_ASSERT_NOT_NULL(coreDump);

    //
    //  Step 1, ask what there is to retrieve, and step 2, stop if there is nothing.
    //
    const uint32_t heapBeforeInformation = esp_get_free_heap_size();
    Esp32Messaging::CoreDumpInformationResponse *information = GetCoreDumpInformation(dispatcher, coreDump);
    TEST_ASSERT_NOT_NULL(information);

    const uint8_t isValid = information->IsValid;
    const uint32_t coreDumpSize = information->CoreDumpSize;
    const uint32_t partitionSize = information->PartitionSize;
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&information));

    AssertHeapRecovered(heapBeforeInformation, "getting the core dump information");

    TEST_ASSERT_TRUE_MESSAGE(isValid != 0, "There is no valid core dump to retrieve, so run \"Generate a core dump\" first.");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, coreDumpSize, "The core dump is empty, so run \"Generate a core dump\" first.");
    TEST_ASSERT_EQUAL(CORE_DUMP_PARTITION_SIZE, partitionSize);
    TEST_ASSERT_LESS_OR_EQUAL(partitionSize, coreDumpSize);

    //
    //  Steps 3 and 4, ask for the dump a fragment at a time until it has all been
    //  retrieved.  The request is made once and reused, as the handler takes the payload
    //  of each request and gives the response back in the same message.
    //
    //  The last fragment is short of the full 4000 bytes unless the dump divides
    //  exactly.  A fragment has to lie wholly inside the dump, so that last one is asked
    //  for at the size it actually is rather than at the size of all the others.
    //
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, StatusCodes::CompletedOk, MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    const uint32_t heapBeforeFragments = esp_get_free_heap_size();
    uint32_t retrieved = 0;
    uint32_t fragments = 0;
    uint32_t lowestHeap = heapBeforeFragments;

    printf("Retrieving %" PRIu32 " bytes of core dump in fragments of %u bytes, free heap %" PRIu32 " bytes.\n", coreDumpSize, CORE_DUMP_FRAGMENT_SIZE, heapBeforeFragments);

    while (retrieved < coreDumpSize)
    {
        const uint32_t remaining = coreDumpSize - retrieved;
        const uint32_t wanted = (remaining < CORE_DUMP_FRAGMENT_SIZE) ? remaining : CORE_DUMP_FRAGMENT_SIZE;
        const uint32_t heapBeforeFragment = esp_get_free_heap_size();

        Esp32Messaging::CoreDumpFragmentResponse *fragment = GetCoreDumpFragment(dispatcher, coreDump, request, retrieved, wanted);
        TEST_ASSERT_NOT_NULL(fragment);
        TEST_ASSERT_EQUAL(wanted, fragment->Size);
        TEST_ASSERT_NOT_NULL(fragment->Data);
        TestEncoders::FreeCoreDumpFragmentResponse(&fragment);

        retrieved += wanted;
        fragments++;

        //
        //  Checked one fragment at a time as well as over the run as a whole, so that a
        //  leak is reported against the request that caused it rather than at the end.
        //
        const uint32_t heapAfterFragment = esp_get_free_heap_size();
        if (heapAfterFragment < lowestHeap)
        {
            lowestHeap = heapAfterFragment;
        }
        AssertHeapRecovered(heapBeforeFragment, "retrieving a core dump fragment");

        if ((fragments % PROGRESS_INTERVAL) == 0)
        {
            printf("  %" PRIu32 " fragments, %" PRIu32 " of %" PRIu32 " bytes, free heap %" PRIu32 " bytes, lowest %" PRIu32 ".\n", fragments, retrieved, coreDumpSize, heapAfterFragment, lowestHeap);
        }
    }

    const uint32_t heapAfterFragments = esp_get_free_heap_size();

    printf("Retrieved %" PRIu32 " bytes in %" PRIu32 " fragments.\n", retrieved, fragments);
    printf("Free heap %" PRIu32 " bytes at the start, %" PRIu32 " at the end, lowest %" PRIu32 ".\n", heapBeforeFragments, heapAfterFragments, lowestHeap);

    TEST_ASSERT_EQUAL(coreDumpSize, retrieved);
    AssertHeapRecovered(heapBeforeFragments, "retrieving the whole core dump");

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
    //
    //  With the dump erased there is nothing to hand back, and the two ways the handler
    //  can tell give different answers.  A request at offset zero works the checksum out
    //  and refuses with CrcError.  A request at any other offset does not, so what
    //  catches it is the length at the head of the partition, which an erase leaves
    //  reading as 0xffffffff and no core dump can be that long.
    //
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, StatusCodes::CompletedOk, MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    const uint32_t heapBefore = esp_get_free_heap_size();
    AssertCoreDumpFragmentRefused(dispatcher, coreDump, request, 0, CORE_DUMP_FRAGMENT_SIZE, StatusCodes::CrcError);
    AssertCoreDumpFragmentRefused(dispatcher, coreDump, request, CORE_DUMP_FRAGMENT_SIZE, CORE_DUMP_FRAGMENT_SIZE, StatusCodes::Failure);
    AssertHeapRecovered(heapBefore, "refusing fragments of an erased core dump");

    Message::DeleteMessage(request);

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
    //  A value of 20 gives a dump of about 166 KBytes.  The depth sets how many stack
    //  frames are recorded, but most of that comes from the development configuration
    //  putting the RAM contents in the dump as well, which is the size the retrieval
    //  tests want to be reading back.
    //
    GenerateCoreDump(20);
}
