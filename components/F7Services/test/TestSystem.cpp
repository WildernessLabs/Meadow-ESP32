/*
 *  TestSystem.cpp
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

#include "TestAssertions.hpp"
#include "TestEncoders.hpp"
#include "TestHarness.hpp"

//
//  [HardwareOnly] marks a test that cannot run under emulation.  This one reads a
//  real ADC; under QEMU the read never returns.
//
#define CLASS_OF_TESTS "[System][HardwareOnly]"

/**
 * @brief Unit test for the battery charge level method.
 */
TEST_CASE("Get battery level", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
    TEST_ASSERT_NOT_NULL(dispatcher);

    Message *getBatteryChargeLevel = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::System, SystemFunction::GetBatteryChargeLevel, StatusCodes::CompletedOk, 0xff, nullptr, 0);
    TEST_ASSERT_NOT_NULL(getBatteryChargeLevel);

    dispatcher->QueueMessageForEsp32(getBatteryChargeLevel);

    WaitForResponse(dispatcher, 1000);
    ASSERT_OUTBOUND_QUEUE_LENGTH(dispatcher, 1);

    Message *response = dispatcher->GetOutboundMessage();
    ASSERT_RESPONSE(response, Esp32Interfaces::System, SystemFunction::GetBatteryChargeLevel, StatusCodes::CompletedOk);
    ASSERT_HAS_PAYLOAD(response);

    Esp32Messaging::IntegerResponse *integerResponse = TestEncoders::ExtractIntegerResponse(response->Payload);
    TEST_ASSERT_NOT_NULL(integerResponse);
    TEST_ASSERT_GREATER_THAN(0, integerResponse->Result);
    printf("Battery level: %" PRIi32 " mV\n", integerResponse->Result);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&integerResponse));

    Message::DeleteMessage(response);

    ReleaseMessageDispatcher();
}
