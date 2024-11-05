/**
 * @brief Tests the Bluetooth system.
 */
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <string.h>
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "MockTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "test_common.hpp"
#include "test_bluetooth.hpp"

/**
 *  Convert the BTStackConfig object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedBTStackConfigBufferSize method.
 *
 *  @param bTStackConfig
 *      BTStackConfig object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BTStackConfig object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeBTStackConfig
void EncodeBTStackConfig(Esp32Messaging::BTStackConfig *bTStackConfig, uint8_t *buffer)
{
    Encoders::EncodeString(bTStackConfig->Config, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BTStackConfig object.
 *
 *  @param bTStackConfig
 *      BTStackConfig object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded BTStackConfig object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedBTStackConfigBufferSize
int EncodedBTStackConfigBufferSize(Esp32Messaging::BTStackConfig *bTStackConfig)
{
    int result = 0;
    result += Encoders::StringLength(bTStackConfig->Config);
    return (result + 1);
}

/**
 * @brief
 */
TEST_CASE("Start Bluetooth", "[Bluetooth]")
{
    MockMessageDispatcher *messageDispatcher = new MockMessageDispatcher(nullptr);
    TEST_ASSERT_NOT_NULL(messageDispatcher);

    Esp32Messaging::BTStackConfig stackConfig;
    stackConfig.Config = (char *) json_test;

    Message startMessage = {};
    startMessage.MessageType = MessageTypes::Header;
    startMessage.Interface = Esp32Interfaces::BlueTooth;
    startMessage.Function = BluetoothFunction::Start;
    startMessage.PayloadLength = EncodedBTStackConfigBufferSize(&stackConfig);
    TEST_ASSERT_GREATER_THAN(0, startMessage.PayloadLength);
    startMessage.Payload = (uint8_t *) pvPortMalloc(startMessage.PayloadLength);
    TEST_ASSERT_NOT_NULL(startMessage.Payload);
    EncodeBTStackConfig(&stackConfig, startMessage.Payload);

    messageDispatcher->QueueMessageForEsp32(&startMessage);

    WaitForResponse(messageDispatcher, 10000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_NULL(response->Payload);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::BlueTooth, response->Interface);
    TEST_ASSERT_EQUAL(BluetoothFunction::Start, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
}