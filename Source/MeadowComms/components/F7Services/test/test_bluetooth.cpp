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
 *  @brief Common message dispatcher for these tests.
 */
static MockMessageDispatcher *messageDispatcher = nullptr;

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
 * @brief Start the Bluetooth service using the specified configuration.
 *
 * @param configuration Configuration of the service.
 */
static void StartBlueToothService(const char *const configuration)
{
    if (messageDispatcher == nullptr)
    {
        messageDispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(messageDispatcher);
    }

    Esp32Messaging::BTStackConfig stackConfig;
    stackConfig.Config = (char *) configuration;

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

/**
 * @brief Stop the Bluetooth service.
 */
static void StopBluetoothService()
{
    if (messageDispatcher == nullptr)
    {
        messageDispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(messageDispatcher);
    }

    Message stopMessage = {};
    stopMessage.MessageType = MessageTypes::Header;
    stopMessage.Interface = Esp32Interfaces::BlueTooth;
    stopMessage.Function = BluetoothFunction::Stop;
    stopMessage.PayloadLength = 0;
    stopMessage.Payload = nullptr;

    messageDispatcher->QueueMessageForEsp32(&stopMessage);

    WaitForResponse(messageDispatcher, 10000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_NULL(response->Payload);
    TEST_ASSERT_EQUAL(0, response->PayloadLength);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::BlueTooth, response->Interface);
    TEST_ASSERT_EQUAL(BluetoothFunction::Stop, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
}

/**
 * @brief Test starting a Bluetooth service for MeadowA, Company ID Espressif (0x02e5).
 */
TEST_CASE("Start Bluetooth (MeadowA, Espressif)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowA, Company ID Espressif (0x02e5)\n");
    StartBlueToothService(meadow_a_json);
}

/**
 * @brief Test starting a Bluetooth service for MeadowB, Company ID BaiDu (0x011c).
 */
TEST_CASE("Start Bluetooth (MeadowB, Baidu)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowB, Company ID Baidu (0x011c)\n");
    StartBlueToothService(meadow_b_json);
}

/**
 * @brief Test starting a Bluetooth service for MeadowC with no company identifier.
 */
TEST_CASE("Start Bluetooth (MeadowC, no company identifier)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for (MeadowC, no company identifier)\n");
    StartBlueToothService(meadow_c_json);
}

/**
 * @brief Test stopping the Bluetooth service when no service has been started.
 */
TEST_CASE("Stop Bluetooth service (no service started)", "[Bluetooth]")
{
    printf("Stopping Bluetooth service\n");
    StopBluetoothService();
}

/**
 * @brief Test the stop functionality when Bluetooth has been started.
 */
TEST_CASE("Stop Bluetooth Service (will start MeadowA service first)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowA, Company ID Espressif (0x02e5)\n");
    StartBlueToothService(meadow_a_json);

    printf("Waiting for 1 second before stopping Bluetooth service\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Stopping Bluetooth service\n");
    StopBluetoothService();
}

/**
 * @brief Test restarting the Bluetooth service.
 */
TEST_CASE("Restart Bluetooth service with different configuration", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowA, Company ID Espressif (0x02e5)\n");
    StartBlueToothService(meadow_a_json);

    printf("Waiting for 1 second before stopping Bluetooth service\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Stopping Bluetooth service\n");
    StopBluetoothService();

    printf("Starting Bluetooth service for MeadowB, Company ID Baidu (0x011c)\n");
    StartBlueToothService(meadow_b_json);
}