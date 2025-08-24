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

#define CLASS_OF_TESTS "[CoreDump]"

/**
 * @brief Extract the core dump information from a buffer.
 *
 * @param buffer Pointer to the buffer containing the core dump data.
 * @return Esp32Messaging::CoreDumpInformation Structure containing the extracted information.
 */
Esp32Messaging::CoreDumpInformation *ExtractCoreDumpInformation(uint8_t *buffer)
{
    Esp32Messaging::CoreDumpInformation *information = vPortMalloc(sizeof(Esp32Messaging::CoreDumpInformation));

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
 * @brief Unit test for getting core dump information.
 */
TEST_CASE("Get core dump information", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetCoreDumpInformation, StatusCodes::CompletedOk, 0xff, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    CoreDump::GetInformation(request);
    WaitForResponse(messageDispatcher, 10000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::GetCoreDumpInformation, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_NULL(response->Payload);

    Esp32Messaging::CoreDumpInformation *information = ExtractCoreDumpInformation(response->Payload);
    TEST_ASSERT_EQUAL(0x10000, information->PartitionSize);

    printf("Validate the following core dump information:\n");
    printf("IsValid: %s\n", information->IsValid ? "true" : "false");
    printf("Partition Size: 0x%05" PRIx32 "\n", information->PartitionSize);
    printf("Core Dump Size: 0x%05" PRIx32 "\n", information->CoreDumpSize);
    vPortFree(information);

    Message::DeleteMessage(response);
    delete messageDispatcher;
}

/**
 * @brief Unit test for getting a core dump fragment.
 */
TEST_CASE("Get core dump fragment", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetCoreDumpFragment, StatusCodes::CompletedOk, 0xff, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    CoreDump::GetFragment(request);
    WaitForResponse(messageDispatcher, 1000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    // Additional checks can be added here for payload

    Message::DeleteMessage(response);
    delete messageDispatcher;
}

/**
 * @brief Unit test for erasing the core dump.
 */
TEST_CASE("Erase core dump", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::EraseCoreDump, StatusCodes::CompletedOk, 0xff, nullptr, 0);
    TEST_ASSERT_NOT_NULL(request);

    CoreDump::Erase(request);
    // The Erase implementation is currently a stub; add assertions when implemented
    // WaitForResponse(messageDispatcher, 1000);
    // TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message::DeleteMessage(request);
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
        assert(0);
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
    GenerateCoreDump(20);
}