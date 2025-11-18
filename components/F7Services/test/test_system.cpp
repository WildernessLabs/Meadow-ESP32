/*
 *  test_system.cpp
 *
 *  Implementation of the test methods for the SystemRequestHandler class.
 */

#include "sdkconfig.h"

#include <stdio.h>
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "Utils.hpp"

#include "test_common.hpp"

#define CLASS_OF_TESTS "[System]"

/**
 * @brief Unit test for the battery charge level method.
 */
TEST_CASE("Get battery level", CLASS_OF_TESTS)
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    Message *getBatteryChargeLevel = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetBatteryChargeLevel, StatusCodes::CompletedOk, 0xff, nullptr, 0);
    TEST_ASSERT_NOT_NULL(getBatteryChargeLevel);

    messageDispatcher->QueueMessageForEsp32(getBatteryChargeLevel);

    WaitForResponse(messageDispatcher, 1000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::System, response->Interface);
    TEST_ASSERT_EQUAL(SystemFunction::GetBatteryChargeLevel, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    TEST_ASSERT_GREATER_THAN(0, response->PayloadLength);
    TEST_ASSERT_NOT_NULL(response->Payload);

    Esp32Messaging::IntegerResponse *integerResponse = ExtractIntegerResponse(response->Payload);
    TEST_ASSERT_NOT_NULL(integerResponse);
    TEST_ASSERT_GREATER_THAN(0, integerResponse->Result);
    printf("Battery level: %ld mV\n", integerResponse->Result);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&integerResponse));

    Message::DeleteMessage(response);
    response = nullptr;

    delete messageDispatcher;
}
