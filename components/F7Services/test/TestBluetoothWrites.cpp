/*
 *  TestBluetoothWrites.cpp
 *
 *  Tests that client writes reach the STM32 whatever form they take (issue #299).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "unity.h"

#include "BluetoothRequestHandler.hpp"
#include "MockMessageDispatcher.hpp"
#include "TestAssertions.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Bluetooth][Emulated]"

/**
 * @brief Attribute handle the tests write to.
 */
#define WRITE_HANDLE 0x002a

/**
 * @brief Handle of the descriptor on the characteristic ConfigureCharacteristic makes.
 */
#define DESCRIPTOR_HANDLE (WRITE_HANDLE + 1)

/**
 * @brief Length of the long value the prepared write tests send: more than one packet holds.
 */
#define LONG_VALUE_LENGTH 300

/**
 * @brief Length of each piece the long value is sent in.
 */
#define PIECE_LENGTH 100

/**
 * @brief Gives the tests access to BluetoothRequestHandler's write handling.
 */
class TestBluetoothWrites
{
public:
    /**
     * @brief Make a handler of the test's own that raises its events through dispatcher.
     *
     *  The handler is created without Setup(), so it starts no task and no Bluetooth stack.
     *  Any handler the test dispatcher already holds is set aside and put back by Release().
     *
     * @param dispatcher Dispatcher the handler raises its events through.
     */
    static void Create(MockMessageDispatcher *dispatcher)
    {
        //
        //  The dispatcher is a static shared by every handler, so the handler made here
        //  replaces it for all of them until Release() puts it back.
        //
        _savedInstance = BluetoothRequestHandler::_instance;
        _savedInstantiated = BluetoothRequestHandler::_instantiated;
        _savedDispatcher = BluetoothRequestHandler::_messageDispatcher;
        BluetoothRequestHandler::_instance = nullptr;
        BluetoothRequestHandler::_instantiated = false;
        BluetoothRequestHandler::_instance = new BluetoothRequestHandler(dispatcher);
        TEST_ASSERT_NOT_NULL(BluetoothRequestHandler::_instance);
    }

    /**
     * @brief Delete the handler made by Create() and put back the one it set aside.
     */
    static void Release()
    {
        BluetoothRequestHandler::DiscardPreparedWrite();
        delete BluetoothRequestHandler::_instance;
        BluetoothRequestHandler::_instance = _savedInstance;
        BluetoothRequestHandler::_instantiated = _savedInstantiated;
        BluetoothRequestHandler::_messageDispatcher = _savedDispatcher;
        _savedInstance = nullptr;
        _savedDispatcher = nullptr;
    }

    /**
     * @brief Pass a write event to the GATT callback, as Bluedroid does.
     *
     *  need_rsp is false, so the callback sends no response and needs no Bluetooth stack.
     *
     * @param handle Attribute written.
     * @param value Bytes written.
     * @param length Number of bytes written.
     */
    static void WriteWithoutResponse(const uint16_t handle, uint8_t *value, const uint16_t length)
    {
        esp_ble_gatts_cb_param_t param = {};
        param.write.handle = handle;
        param.write.offset = 0;
        param.write.need_rsp = false;
        param.write.is_prep = false;
        param.write.len = length;
        param.write.value = value;
        BluetoothRequestHandler::GATTSProfileEventHandler(ESP_GATTS_WRITE_EVT, 0, &param);
    }

    /**
     * @brief Pass one prepared write piece to the GATT callback, as Bluedroid does.
     *
     *  need_rsp is false so the callback sends no response.  A real client asks for one,
     *  which the stack sends; that part needs a running stack and is left to the board.
     *
     * @param offset Offset of the piece in the value.
     * @param value Bytes of the piece.
     * @param length Number of bytes in the piece.
     */
    static void PreparedWriteEvent(const uint16_t offset, uint8_t *value, const uint16_t length)
    {
        esp_ble_gatts_cb_param_t param = {};
        param.write.handle = WRITE_HANDLE;
        param.write.offset = offset;
        param.write.need_rsp = false;
        param.write.is_prep = true;
        param.write.len = length;
        param.write.value = value;
        BluetoothRequestHandler::GATTSProfileEventHandler(ESP_GATTS_WRITE_EVT, 0, &param);
    }

    /**
     * @brief Pass an execute write event to the GATT callback, as Bluedroid does.
     *
     *  The callback also answers the client, which fails harmlessly here with no stack.
     *
     * @param flag ESP_GATT_PREP_WRITE_EXEC or ESP_GATT_PREP_WRITE_CANCEL.
     */
    static void ExecuteWriteEvent(const uint8_t flag)
    {
        esp_ble_gatts_cb_param_t param = {};
        param.exec_write.exec_write_flag = flag;
        BluetoothRequestHandler::GATTSProfileEventHandler(ESP_GATTS_EXEC_WRITE_EVT, 0, &param);
    }

    /**
     * @brief Configure one characteristic, with one descriptor, and give its attributes handles.
     *
     *  The characteristic's value gets WRITE_HANDLE and its descriptor DESCRIPTOR_HANDLE,
     *  as Bluedroid would number the attribute table.
     */
    static void ConfigureCharacteristic()
    {
        const char *configuration = "{\"deviceName\":\"MeadowTest\",\"services\":[{\"name\":\"S\",\"uuid\":254,\"characteristics\":[{\"uuid\":10755,\"permission\":17,\"props\":14,"
                                    "\"len\":4,\"descriptors\":[{\"uuid\":10498,\"permission\":17,\"len\":2,\"value\":0}]}]}]}";
        TEST_ASSERT_TRUE_MESSAGE(BluetoothRequestHandler::_manager.BuildGraphFromJson(configuration), "The configuration was not parsed.");
        const uint16_t handles[] = {WRITE_HANDLE - 2, WRITE_HANDLE - 1, WRITE_HANDLE, DESCRIPTOR_HANDLE};
        TEST_ASSERT_TRUE_MESSAGE(BluetoothRequestHandler::_manager.SetHandles(handles, sizeof(handles) / sizeof(handles[0])), "The handles were not set.");
    }

    /**
     * @brief Call the private PrepareWrite method.
     */
    static esp_gatt_status_t Prepare(const uint16_t handle, const uint16_t offset, const uint8_t *value, const uint16_t length)
    {
        return (BluetoothRequestHandler::PrepareWrite(handle, offset, value, length));
    }

    /**
     * @brief Call the private ExecuteWrite method.
     */
    static void Execute(const bool execute)
    {
        BluetoothRequestHandler::ExecuteWrite(execute);
    }

    /**
     * @brief Whether a prepared write is being collected.
     */
    static bool PreparedWriteInProgress()
    {
        return (BluetoothRequestHandler::_preparedWriteValue != nullptr);
    }

private:
    /**
     * @brief Handler set aside while the test's own is in use.
     */
    static BluetoothRequestHandler *_savedInstance;

    /**
     * @brief Instantiation guard set aside while the test's own handler is in use.
     */
    static bool _savedInstantiated;

    /**
     * @brief Shared dispatcher set aside while the test's own handler is in use.
     */
    static IMessageDispatcher *_savedDispatcher;
};

BluetoothRequestHandler *TestBluetoothWrites::_savedInstance = nullptr;
bool TestBluetoothWrites::_savedInstantiated = false;
IMessageDispatcher *TestBluetoothWrites::_savedDispatcher = nullptr;

/**
 * @brief The dispatcher these tests raise events through.  It lives for the whole run.
 *
 * @return The dispatcher.
 */
static MockMessageDispatcher *Dispatcher()
{
    static MockMessageDispatcher *dispatcher = nullptr;
    if (dispatcher == nullptr)
    {
        dispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(dispatcher);
    }
    return (dispatcher);
}

/**
 * @brief Release any events earlier tests left with the dispatcher.
 *
 * @param dispatcher Dispatcher to empty.
 */
static void DrainDispatcher(MockMessageDispatcher *dispatcher)
{
    Message *message = dispatcher->GetOutboundMessage();
    while (message != nullptr)
    {
        Message::DeleteMessage(message);
        message = dispatcher->GetOutboundMessage();
    }
}

/**
 * @brief Check the next event is a client write of the expected value, and release it.
 *
 * @param dispatcher Dispatcher holding the event.
 * @param expected Value expected.
 * @param length Length of expected.
 */
static void CheckClientWrite(MockMessageDispatcher *dispatcher, const uint8_t *expected, const uint32_t length)
{
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "The write did not reach the STM32.");
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, StatusCodes::CompletedOk);
    TEST_ASSERT_EQUAL_UINT16(WRITE_HANDLE, Encoders::ExtractUInt16(event->Payload));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(length, Encoders::ExtractUInt32(event->Payload + 2), "The value written has the wrong length.");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, event->Payload + 6, length, "The value written was not delivered intact.");
    Message::DeleteMessage(event);
}

/**
 * @brief Fill a buffer with a recognisable pattern.
 *
 * @param buffer Buffer to fill.
 * @param length Length of buffer.
 */
static void FillPattern(uint8_t *buffer, const uint32_t length)
{
    for (uint32_t index = 0; index < length; index++)
    {
        buffer[index] = static_cast<uint8_t>((index * 3) + 1);
    }
}

TEST_CASE("A write without response reaches the STM32", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    TestBluetoothWrites::Create(dispatcher);

    TestBluetoothWrites::ConfigureCharacteristic();
    uint8_t value[] = {0x01, 0x02, 0x03, 0x04};
    TestBluetoothWrites::WriteWithoutResponse(WRITE_HANDLE, value, sizeof(value));
    CheckClientWrite(dispatcher, value, sizeof(value));

    TestBluetoothWrites::Release();
}

/**
 * @brief Check that a descriptor write Bluedroid has already answered is not passed to the STM32.
 *
 *  A client subscribes by writing a descriptor.  Bluedroid answers descriptor writes itself
 *  and reports them with need_rsp false, like a write without response.  They were never
 *  passed to the STM32, and passing them now would raise events nothing expects.
 */
TEST_CASE("A descriptor write answered by the stack is not passed to the STM32", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    TestBluetoothWrites::Create(dispatcher);

    TestBluetoothWrites::ConfigureCharacteristic();
    uint8_t subscribe[] = {0x01, 0x00};
    TestBluetoothWrites::WriteWithoutResponse(DESCRIPTOR_HANDLE, subscribe, sizeof(subscribe));
    TEST_ASSERT_NULL_MESSAGE(dispatcher->GetOutboundMessage(), "A descriptor write was passed to the STM32.");

    TestBluetoothWrites::Release();
}

TEST_CASE("A long write passed to the GATT callback reaches the STM32 when executed", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    TestBluetoothWrites::Create(dispatcher);

    uint8_t value[LONG_VALUE_LENGTH];
    FillPattern(value, sizeof(value));
    for (uint16_t offset = 0; offset < LONG_VALUE_LENGTH; offset += PIECE_LENGTH)
    {
        TestBluetoothWrites::PreparedWriteEvent(offset, value + offset, PIECE_LENGTH);
    }
    TEST_ASSERT_NULL_MESSAGE(dispatcher->GetOutboundMessage(), "A piece was delivered before the write was executed.");
    TestBluetoothWrites::ExecuteWriteEvent(ESP_GATT_PREP_WRITE_EXEC);
    CheckClientWrite(dispatcher, value, sizeof(value));

    TestBluetoothWrites::Release();
}

TEST_CASE("A long write sent as prepared writes reaches the STM32 as one value", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    TestBluetoothWrites::Create(dispatcher);

    uint8_t value[LONG_VALUE_LENGTH];
    FillPattern(value, sizeof(value));
    //
    //  The pieces may arrive in any order.
    //
    const uint16_t offsets[] = {PIECE_LENGTH, 0, 2 * PIECE_LENGTH};
    for (const uint16_t offset: offsets)
    {
        TEST_ASSERT_EQUAL_MESSAGE(ESP_GATT_OK, TestBluetoothWrites::Prepare(WRITE_HANDLE, offset, value + offset, PIECE_LENGTH), "A piece of the long write was refused.");
        TEST_ASSERT_NULL_MESSAGE(dispatcher->GetOutboundMessage(), "A piece was delivered before the write was executed.");
    }
    TestBluetoothWrites::Execute(true);
    CheckClientWrite(dispatcher, value, sizeof(value));
    TEST_ASSERT_FALSE_MESSAGE(TestBluetoothWrites::PreparedWriteInProgress(), "The prepared write was kept after it was executed.");

    TestBluetoothWrites::Release();
}

TEST_CASE("A cancelled prepared write is not delivered", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    TestBluetoothWrites::Create(dispatcher);

    uint8_t value[PIECE_LENGTH];
    FillPattern(value, sizeof(value));
    TEST_ASSERT_EQUAL(ESP_GATT_OK, TestBluetoothWrites::Prepare(WRITE_HANDLE, 0, value, sizeof(value)));
    TestBluetoothWrites::Execute(false);
    TEST_ASSERT_NULL_MESSAGE(dispatcher->GetOutboundMessage(), "A cancelled write was delivered.");
    TEST_ASSERT_FALSE_MESSAGE(TestBluetoothWrites::PreparedWriteInProgress(), "The prepared write was kept after it was cancelled.");

    TestBluetoothWrites::Release();
}

TEST_CASE("Prepared writes outside the attribute or to a second attribute are refused", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    TestBluetoothWrites::Create(dispatcher);

    uint8_t value[PIECE_LENGTH];
    FillPattern(value, sizeof(value));
    TEST_ASSERT_EQUAL_MESSAGE(ESP_GATT_INVALID_OFFSET, TestBluetoothWrites::Prepare(WRITE_HANDLE, ESP_GATT_MAX_ATTR_LEN + 1, value, 1), "An offset past the longest attribute was accepted.");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_GATT_INVALID_ATTR_LEN, TestBluetoothWrites::Prepare(WRITE_HANDLE, ESP_GATT_MAX_ATTR_LEN - 10, value, 11), "A piece ending past the longest attribute was accepted.");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_GATT_OK, TestBluetoothWrites::Prepare(WRITE_HANDLE, ESP_GATT_MAX_ATTR_LEN - 10, value, 10), "A piece ending at the longest attribute was refused.");
    TEST_ASSERT_EQUAL_MESSAGE(ESP_GATT_PREPARE_Q_FULL, TestBluetoothWrites::Prepare(WRITE_HANDLE + 1, 0, value, 1), "A prepared write to a second attribute was accepted.");
    TestBluetoothWrites::Execute(false);

    TestBluetoothWrites::Release();
}
