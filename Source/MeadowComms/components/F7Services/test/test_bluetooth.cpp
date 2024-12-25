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
#include "Logging.hpp"

#include "test_common.hpp"
#include "test_bluetooth.hpp"

const char *ON_OFF_NAME = "OnOff";
const char *ON_OFF_UUID = "016e99d6-8a61-11eb-8dcd-0242ac1300ff";

const char *BOOLEAN_FIELD_NAME = "BooleanField";
const char *BOOLEAN_FIELD_UUID = "017e99d6-8a61-11eb-8dcd-0242ac1300aa";

const char *NUMBER_FIELD_NAME = "NumberField";
const char *NUMBER_FIELD_UUID = "018e99d6-8a61-11eb-8dcd-0242ac1300bb";

const char *TEXT_FIELD_NAME = "TextField";
const char *TEXT_FIELD_UUID = "019e99d6-8a61-11eb-8dcd-0242ac1300cc";

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
 *  Convert the BTServerDataSet object into a byte stream that can be sent to the STM32.
 *
 *  The amount of memory required can be obtained by calling the EncodedBTServerDataSetBufferSize method.
 *
 *  @param bTServerDataSet
 *      BTServerDataSet object to be encoded.
 *
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BTServerDataSet object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeBTServerDataSet
void EncodeBTServerDataSet(Esp32Messaging::BTServerDataSet *bTServerDataSet, uint8_t *buffer)
{
    Encoders::EncodeUInt16(bTServerDataSet->Handle, buffer);
    buffer += 2;
    Encoders::EncodeUInt32(bTServerDataSet->SetDataLength, buffer);
    buffer += 4;
    if (bTServerDataSet->SetDataLength > 0)
    {
        memcpy((void *) buffer, (void *) bTServerDataSet->SetData, bTServerDataSet->SetDataLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BTServerDataSet object.
 *
 *  @param bTServerDataSet
 *      BTServerDataSet object to be encoded.
 *
 *  @returns
 *      Number of bytes required to hold the encoded BTServerDataSet object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedBTServerDataSetBufferSize
int EncodedBTServerDataSetBufferSize(Esp32Messaging::BTServerDataSet *bTServerDataSet)
{
    int result = 0;
    result += bTServerDataSet->SetDataLength;
    return (result + 6);
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

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStart();
    }

    StartBlueToothService(meadow_a_json);

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStop();
        Logging::DumpStandaloneHeapTraceData();
    }
}

/**
 * @brief Test starting a Bluetooth service for MeadowB, Company ID BaiDu (0x011c).
 */
TEST_CASE("Start Bluetooth (MeadowB, Baidu)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowB, Company ID Baidu (0x011c)\n");

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStart();
    }

    StartBlueToothService(meadow_b_json);

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStop();
        Logging::DumpStandaloneHeapTraceData();
    }
}

/**
 * @brief Test starting a Bluetooth service for MeadowC with no company identifier.
 */
TEST_CASE("Start Bluetooth (MeadowC, no company identifier)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for (MeadowC, no company identifier)\n");

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStart();
    }

    StartBlueToothService(meadow_c_json);

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStop();
        Logging::DumpStandaloneHeapTraceData();
    }
}

/**
 * @brief Test starting a Bluetooth service for MeadowA, Company ID Espressif (0x02e5).
 */
TEST_CASE("Start Bluetooth (MeadowF7, Espressif)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowF7 (Bluetooth_Basics), Company ID Espressif (0x02e5)\n");

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStart();
    }

    StartBlueToothService(meadow_f7_json);

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStop();
        Logging::DumpStandaloneHeapTraceData();
    }
}

/**
 * @brief Test stopping the Bluetooth service when no service has been started.
 */
TEST_CASE("Stop Bluetooth service (no service started)", "[Bluetooth]")
{
    printf("Stopping Bluetooth service\n");
    StopBluetoothService();

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStop();
        Logging::DumpStandaloneHeapTraceData();
    }
}

/**
 * @brief Test the stop functionality when Bluetooth has been started.
 */
TEST_CASE("Stop Bluetooth Service (will start MeadowA service first)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowA, Company ID Espressif (0x02e5)\n");

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStart();
    }

    StartBlueToothService(meadow_a_json);

    printf("Waiting for 1 second before stopping Bluetooth service\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Stopping Bluetooth service\n");
    StopBluetoothService();

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStop();
        Logging::DumpStandaloneHeapTraceData();
    }
}

/**
 * @brief Test restarting the Bluetooth service.
 */
TEST_CASE("Restart Bluetooth service with different configuration", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowA, Company ID Espressif (0x02e5)\n");

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStart();
    }

    StartBlueToothService(meadow_a_json);

    printf("Waiting for 1 second before stopping Bluetooth service\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Stopping Bluetooth service\n");
    StopBluetoothService();

    printf("Starting Bluetooth service for MeadowB, Company ID Baidu (0x011c)\n");
    StartBlueToothService(meadow_b_json);

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStop();
        Logging::DumpStandaloneHeapTraceData();
    }
}

/**
 * @brief Test starting a Bluetooth service for MeadowF7, Company ID Espressif (0x02e5) and then accepting
 *        multiple write requests.
 */
TEST_CASE("Soak test writing numeric characteristic, 10,000 iterations (MeadowF7, Espressif)", "[Bluetooth]")
{
    printf("Starting Bluetooth service for MeadowF7 (Bluetooth_Basics), Company ID Espressif (0x02e5)\n");

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStart();
    }

    StartBlueToothService(meadow_f7_json);

    Message message = {};
    message.MessageType = MessageTypes::Header;
    message.Interface = Esp32Interfaces::BlueTooth;
    message.Function = BluetoothFunction::GetHandles;
    message.PayloadLength = 0;
    message.Payload = nullptr;
    messageDispatcher->QueueMessageForEsp32(&message);

    WaitForResponse(messageDispatcher, 10000);
    TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());

    Message *response = messageDispatcher->GetOutboundMessage();
    //
    //  The handles are all uint16_t values.  The first value will be the handle for the service.
    //  Service handles will be followed by the handle for the declaration followed by the handle
    //  for the value.
    //
    //  We have one service and four characteristics giving a payload length of 2 * (1 + (4 * (1 + 1))), so 34 bytes.
    //
    TEST_ASSERT_NOT_NULL(response->Payload);
    TEST_ASSERT_EQUAL(24, response->PayloadLength);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    TEST_ASSERT_EQUAL(Esp32Interfaces::BlueTooth, response->Interface);
    TEST_ASSERT_EQUAL(BluetoothFunction::GetHandles, response->Function);
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    //
    //  The characteristic handles for the meadow_f7_json service will be in the following order:
    //      - Service handle
    //      - 016e99d6-8a61-11eb-8dcd-0242ac1300ff (OnOff)
    //      - 017e99d6-8a61-11eb-8dcd-0242ac1300aa (BooleanField)
    //      - 018e99d6-8a61-11eb-8dcd-0242ac1300bb (NumberField)
    //      - 019e99d6-8a61-11eb-8dcd-0242ac1300cc (TextField)
    //
    __attribute__((unused)) uint8_t *payload = response->Payload;
    __attribute__((unused)) uint16_t handle_count = Encoders::ExtractUInt16(response->Payload);
    __attribute__((unused)) uint32_t handles_length = Encoders::ExtractUInt32(response->Payload + 2);
    __attribute__((unused)) uint16_t number_of_handles = Encoders::ExtractUInt16(response->Payload + 6);
    __attribute__((unused)) uint16_t service_handle = Encoders::ExtractUInt16(response->Payload + 8);
    __attribute__((unused)) uint16_t on_off_declaration_handle = Encoders::ExtractUInt16(response->Payload + 10);
    __attribute__((unused)) uint16_t on_off_value_handle = Encoders::ExtractUInt16(response->Payload + 12);
    __attribute__((unused)) uint16_t boolean_field_declaration_handle = Encoders::ExtractUInt16(response->Payload + 14);
    __attribute__((unused)) uint16_t boolean_field_value_handle = Encoders::ExtractUInt16(response->Payload + 16);
    __attribute__((unused)) uint16_t number_field_declaration_handle = Encoders::ExtractUInt16(response->Payload + 16);
    __attribute__((unused)) uint16_t number_field_value_handle = Encoders::ExtractUInt16(response->Payload + 18);
    __attribute__((unused)) uint16_t text_field_declaration_handle = Encoders::ExtractUInt16(response->Payload + 20);
    __attribute__((unused)) uint16_t text_field_value_handle = Encoders::ExtractUInt16(response->Payload + 22);
    message.DeletePayload();

    printf("Start the client application within the next 60 seconds\n");

    uint32_t iteration = 0;
    while (true)
    {
        iteration++;
        if ((iteration % 100 == 0) || (iteration < 10))
        {
            printf("    Iteration %lu\n", iteration);
        }
        //
        // Wait for a change in the number characteristic.
        //
        WaitForResponse(messageDispatcher, 60000);
        TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());
        //
        //  We should get the value as an event, the memory will have been allocated by the
        //  BluetoothRequestHandler so we need to release it.
        //
        response = messageDispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);
        TEST_ASSERT_NOT_NULL(response->Payload);
        TEST_ASSERT_GREATER_OR_EQUAL(7, response->PayloadLength);
        TEST_ASSERT_EQUAL(MessageTypes::Event, response->MessageType);
        TEST_ASSERT_EQUAL(Esp32Interfaces::BlueTooth, response->Interface);
        TEST_ASSERT_EQUAL(BluetoothFunction::ClientWriteRequestEvent, response->Function);
        TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);

        uint16_t handle = Encoders::ExtractUInt16(response->Payload);
        uint32_t length = Encoders::ExtractUInt32(response->Payload + 2);
        uint32_t value = 0;
        for (int index = 0; index < length; index++)
        {
            value |= (response->Payload[6 + index] << (index * 8));
        }
        TEST_ASSERT_EQUAL(number_field_value_handle, handle);
        Message::DeleteMessage(response);
        //
        //  Use the value to determine if we should stop the test.
        //
        if (value == 0)
        {
            break;
        }
        //
        //  Write the value back to the characteristic for the client unit test to pick up and verify the value.
        //
        //  Reuse the message object from the start of this method.
        //
        message = {};
        message.MessageType = MessageTypes::Header;
        message.Interface = Esp32Interfaces::BlueTooth;
        message.Function = BluetoothFunction::ServerDataSet;

        Esp32Messaging::BTServerDataSet dataSet;
        dataSet.Handle = handle;
        dataSet.SetData = (uint8_t *) &value;
        dataSet.SetDataLength = sizeof(value);
        message.PayloadLength = EncodedBTServerDataSetBufferSize(&dataSet);
        uint8_t *payload = (uint8_t *) pvPortMalloc(message.PayloadLength);
        TEST_ASSERT_NOT_NULL(payload);
        EncodeBTServerDataSet(&dataSet, payload);
        message.Payload = payload;

        messageDispatcher->QueueMessageForEsp32(&message);
        //
        //  Wait for the confirmation message that the change has been written back.
        //
        WaitForResponse(messageDispatcher, 60000);
        TEST_ASSERT_EQUAL(1, messageDispatcher->OutboundMessageQueueLength());
        response = messageDispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);
        TEST_ASSERT_NULL(response->Payload);
        TEST_ASSERT_EQUAL(0, response->PayloadLength);
        TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
        TEST_ASSERT_EQUAL(Esp32Interfaces::BlueTooth, response->Interface);
        TEST_ASSERT_EQUAL(BluetoothFunction::ServerDataSet, response->Function);
        TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, response->StatusCode);
    }

    StopBluetoothService();

    if (_generateHeapTraceData)
    {
        Logging::StandaloneHeapTracingStop();
        Logging::DumpStandaloneHeapTraceData();
    }
}