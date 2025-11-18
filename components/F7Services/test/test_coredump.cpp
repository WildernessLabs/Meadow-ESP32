/**
 * @file test_coredump.cpp
 * @author Wilderness Labs
 * @brief Unit tests for core dump handling on the ESP32 application.
 * @version 0.1
 * @date 2025-08-23
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 *
 * @note This file contains unit tests for core dump management and analysis.
 */

#include "sdkconfig.h"
#include <stdio.h>
#include "unity.h"
#include "test_common.hpp"

#include "Esp32Messaging.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "CoreDump.hpp"
#include "Utils.hpp"

#include "test_common.hpp"

/**
 * @brief Class of tests for core dump functionality (shown in the unity interface).
 */
#define CLASS_OF_TESTS "[CoreDump]"

/**
 * @brief Message ID for core dump requests.
 */
#define MESSAGE_ID 0x01

/**
 * @brief Extract the core dump information from a buffer.
 *
 * @param buffer Pointer to the buffer containing the core dump data.
 * @return Esp32Messaging::CoreDumpInformation Structure containing the extracted information.
 */
Esp32Messaging::CoreDumpInformationResponse *ExtractCoreDumpInformation(uint8_t *buffer)
{
    Esp32Messaging::CoreDumpInformationResponse *information = static_cast<Esp32Messaging::CoreDumpInformationResponse *>(pvPortMalloc(sizeof(Esp32Messaging::CoreDumpInformationResponse)));

    if (buffer == NULL)
    {
        information->IsValid = 0;
        information->CoreDumpSize = 0;
        information->PartitionSize = 0;
        return information;
    }

    information->IsValid = *buffer;
    buffer++;
    information->CoreDumpSize = Encoders::ExtractUInt32(buffer);
    buffer += 4;
    information->PartitionSize = Encoders::ExtractUInt32(buffer);

    return information;
}

/**
 * @brief Size of the encoded core dump fragment request.
 *
 * @param request Pointer to the core dump fragment request object.
 * @return uint32_t Number of bytes needed to encode a core dump fragment request.
 */
uint32_t EncodedCoreDumpFragmentRequestBufferSize(Esp32Messaging::CoreDumpFragmentRequest *request)
{
    return 8;
}

/**
 * @brief Encode a core dump fragment request object.
 *
 * @param request Pointer to the core dump fragment request object.
 * @return uint8_t* Pointer to the encoded data.
 */
uint8_t *EncodeCoreDumpFragmentRequest(Esp32Messaging::CoreDumpFragmentRequest *request)
{
    if (request == nullptr)
    {
        return nullptr;
    }

    uint8_t *fragment = static_cast<uint8_t *>(pvPortMalloc(EncodedCoreDumpFragmentRequestBufferSize(request)));
    if (fragment != nullptr)
    {
        uint8_t *buffer = fragment;
        Encoders::EncodeUInt32(request->Offset, buffer);
        buffer += 4;
        Encoders::EncodeUInt32(request->Size, buffer);
    }

    return fragment;
}

/**
 * @brief Extract a core dump fragment response from a buffer.
 *
 * @param buffer Pointer to the buffer containing the core dump fragment response data.
 * @return Esp32Messaging::CoreDumpFragmentResponse* Pointer to the extracted core dump fragment response.
 */
Esp32Messaging::CoreDumpFragmentResponse *ExtractCoreDumpFragmentResponse(uint8_t *buffer)
{
    if (buffer == NULL)
    {
        return (nullptr);
    }

    Esp32Messaging::CoreDumpFragmentResponse *fragmentResponse = static_cast<Esp32Messaging::CoreDumpFragmentResponse *>(pvPortMalloc(sizeof(Esp32Messaging::CoreDumpFragmentResponse)));
    if (fragmentResponse == NULL)
    {
        return (nullptr);
    }

    fragmentResponse->Size = Encoders::ExtractUInt32(buffer);
    buffer += 4;
    if (fragmentResponse->Size > 0)
    {
        fragmentResponse->Data = static_cast<uint8_t *>(pvPortMalloc(fragmentResponse->Size));
        if (fragmentResponse->Data == NULL)
        {
            Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&fragmentResponse));
            return (nullptr);
        }
        memcpy(fragmentResponse->Data, buffer, fragmentResponse->Size);
    }
    else
    {
        fragmentResponse->Data = nullptr;
    }

    return (fragmentResponse);
}

/**
 * @brief Get the Core Dump Information object
 *
 * @param dispatcher Message dispatcher.
 * @param codeDump Core dump object.
 * @return Esp32Messaging::CoreDumpInformationResponse* Pointer to a CoreDumpInformationResponse object.
 */
Esp32Messaging::CoreDumpInformationResponse *GetCoreDumpInformation(MockMessageDispatcher *dispatcher, CoreDump *codeDump)
{
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetCoreDumpInformation, StatusCodes::CompletedOk, MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    codeDump->GetInformation(request);
    WaitForResponse(messageDispatcher, 2000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::GetCoreDumpInformation, response->Function);
    TEST_ASSERT_EQUAL(MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);
    TEST_ASSERT_NOT_NULL(response->Payload);

    Esp32Messaging::CoreDumpInformationResponse *information = ExtractCoreDumpInformation(response->Payload);
    delete response;

    return information;
}

/**
 * @brief Erase the core dump.
 *
 * @param messageDispatcher Message dispatcher.
 * @param codeDump Core dump object.
 */
void EraseCoreDump(MockMessageDispatcher *messageDispatcher, CoreDump *codeDump)
{
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::EraseCoreDump, StatusCodes::CompletedOk, MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    codeDump->Erase(request);
    WaitForResponse(messageDispatcher, 1000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::EraseCoreDump, response->Function);
    TEST_ASSERT_EQUAL(MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    Message::DeleteMessage(response);
}

/**
 * @brief Unit test for getting core dump information.
 */
TEST_CASE("Get core dump information", CLASS_OF_TESTS)
{
    printf("Test: Get core dump information\n");
    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();

    CoreDump *codeDump = new CoreDump(messageDispatcher);
    TEST_ASSERT_NOT_NULL(codeDump);

    Esp32Messaging::CoreDumpInformationResponse *information = GetCoreDumpInformation(messageDispatcher, codeDump);
    TEST_ASSERT_EQUAL(0x10000, information->PartitionSize);

    printf("Validate the following core dump information:\n");
    printf("IsValid: %s\n", information->IsValid ? "true" : "false");
    printf("Partition Size: 0x%05" PRIx32 "\n", information->PartitionSize);
    printf("Core Dump Size: 0x%05" PRIx32 "\n", information->CoreDumpSize);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&information));

    delete codeDump;
    delete messageDispatcher;
}

/**
 * @brief Unit test for getting a core dump fragment.
 */
TEST_CASE("Get core dump fragment", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();

    CoreDump *codeDump = new CoreDump(messageDispatcher);
    TEST_ASSERT_NOT_NULL(codeDump);

    //
    //  First make sure we have a core dump.
    //
    Esp32Messaging::CoreDumpInformationResponse *information = GetCoreDumpInformation(messageDispatcher, codeDump);
    TEST_ASSERT_NOT_NULL(information);
    TEST_ASSERT_NOT_EQUAL(0, information->IsValid);
    TEST_ASSERT_GREATER_THAN(0, information->CoreDumpSize);
    //
    //  Test for NULL Payload handling.
    //
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, StatusCodes::CompletedOk, MESSAGE_ID, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    codeDump->GetFragment(request);
    WaitForResponse(messageDispatcher, 1000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::GetCoreDumpFragment, response->Function);
    TEST_ASSERT_EQUAL(MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(StatusCodes::InvalidRequest, response->StatusCode);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);
    //
    //  Get the first 200 bytes of the core dump.
    //
    Esp32Messaging::CoreDumpFragmentRequest fragmentRequest = {};
    fragmentRequest.Offset = 0;
    fragmentRequest.Size = 256;
    request->PayloadLength = EncodedCoreDumpFragmentRequestBufferSize(&fragmentRequest);
    request->Payload = EncodeCoreDumpFragmentRequest(&fragmentRequest);
    TEST_ASSERT_NOT_NULL(request->Payload);
    codeDump->GetFragment(request);
    WaitForResponse(messageDispatcher, 1000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::GetCoreDumpFragment, response->Function);
    TEST_ASSERT_EQUAL(MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_NOT_NULL(response->Payload);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);
    Esp32Messaging::CoreDumpFragmentResponse *responseFragment = ExtractCoreDumpFragmentResponse(response->Payload);
    response->DeletePayload();
    TEST_ASSERT_NOT_NULL(responseFragment);
    TEST_ASSERT_EQUAL(256, responseFragment->Size);
    TEST_ASSERT_NOT_NULL(responseFragment->Data);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&responseFragment->Data));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&responseFragment));
    //
    //  Next we ask for data from the end of the coredump.  This time we ask for more bytes than are available.
    //  We do this by setting the offset to 100 bytes from the end of the core dump and then ask for 256 bytes.
    //  The system should work out that there are only 100 bytes left and just return the 100 bytes.
    //
    fragmentRequest.Offset = information->CoreDumpSize - 100;
    fragmentRequest.Size = 256;
    request->PayloadLength = EncodedCoreDumpFragmentRequestBufferSize(&fragmentRequest);
    request->Payload = EncodeCoreDumpFragmentRequest(&fragmentRequest);
    TEST_ASSERT_NOT_NULL(request->Payload);
    codeDump->GetFragment(request);
    WaitForResponse(messageDispatcher, 1000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::GetCoreDumpFragment, response->Function);
    TEST_ASSERT_EQUAL(MESSAGE_ID, response->MessageID);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);
    TEST_ASSERT_NOT_NULL(response->Payload);
    responseFragment = ExtractCoreDumpFragmentResponse(response->Payload);
    response->DeletePayload();
    TEST_ASSERT_NOT_NULL(responseFragment);
    TEST_ASSERT_EQUAL(100, responseFragment->Size);
    TEST_ASSERT_NOT_NULL(responseFragment->Data);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&responseFragment->Data));
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&responseFragment));

    Message::DeleteMessage(response);

    delete codeDump;
    delete messageDispatcher;
}

/**
 * @brief Unit test for erasing the core dump.
 */
TEST_CASE("Erase core dump", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();

    CoreDump *codeDump = new CoreDump(messageDispatcher);
    TEST_ASSERT_NOT_NULL(codeDump);

    EraseCoreDump(messageDispatcher, codeDump);

    Esp32Messaging::CoreDumpInformationResponse *information = GetCoreDumpInformation(messageDispatcher, codeDump);
    TEST_ASSERT_NOT_NULL(information);
    TEST_ASSERT_EQUAL(0, information->IsValid);
    TEST_ASSERT_EQUAL(0x10000, information->PartitionSize);
    TEST_ASSERT_EQUAL(0, information->CoreDumpSize);

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&information));

    delete codeDump;
    delete messageDispatcher;
}

/**
 * @brief Generate a core dump.
 *
 * @param count The number of times to recurse before forcing a core dump.
 */
void GenerateCoreDump(uint32_t count)
{
    if (count == 0)
    {
        printf("Forcing core dump...\n");
        // cppcheck-suppress nullPointer
        *((uint32_t *) 0) = 0;
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
    //  A valid of 20 will generate a core dump of about 23 KBytes.
    //
    GenerateCoreDump(20);
}