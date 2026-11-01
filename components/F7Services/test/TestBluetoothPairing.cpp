/*
 *  TestBluetoothPairing.cpp
 *
 *  Tests for pairing and bonding that need no radio (issue #398): the questions a pairing asks
 *  the STM32, the replies it accepts, the bond requests, the subscriptions kept for bonded
 *  clients, and the payloads that carry them.  The stack's callbacks are called directly, as
 *  Bluedroid would call them.
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include <vector>
#include "esp_bt_main.h"
#include "unity.h"

#include "BluetoothPairing.hpp"
#include "BluetoothRequestHandler.hpp"
#include "Encoders.hpp"
#include "MockMessageDispatcher.hpp"
#include "TestAssertions.hpp"
#include "Utils.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Bluetooth][Emulated]"

/**
 * @brief Length of an encoded BTPairingReply: address, accept and passkey.
 */
#define PAIRING_REPLY_LENGTH (Esp32Messaging::BT_ADDRESS_LENGTH + 1 + 4)

/**
 * @brief Length of an encoded BTPairingEventData: address and passkey.
 */
#define PAIRING_EVENT_LENGTH (Esp32Messaging::BT_ADDRESS_LENGTH + 4)

/**
 * @brief Length of an encoded BTAuthenticationCompleteEventData.
 */
#define AUTHENTICATION_EVENT_LENGTH (Esp32Messaging::BT_ADDRESS_LENGTH + 4)

/**
 * @brief Number shown in the numeric comparison and passkey tests.
 */
#define PASSKEY 123456

/**
 * @brief Client the tests pair with.
 */
static const esp_bd_addr_t CLIENT = {0x34, 0x5f, 0x45, 0x01, 0x02, 0x03};

/**
 * @brief Another client, which no question is about.
 */
static const esp_bd_addr_t OTHER_CLIENT = {0x34, 0x5f, 0x45, 0x0a, 0x0b, 0x0c};

/**
 * @brief Gives the tests access to BluetoothRequestHandler's pairing and bonding.
 */
class TestBluetoothPairing
{
public:
    /**
     * @brief Make a handler of the test's own that raises its events through dispatcher.
     *
     *  The handler is created without Setup(), so it starts no task and no Bluetooth stack.
     *  Any handler the test dispatcher already holds is set aside and put back by Release().
     *
     * @param dispatcher Dispatcher the handler raises its events and responses through.
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
        BluetoothRequestHandler::_pairing.Reset();
    }

    /**
     * @brief Delete the handler made by Create() and put back the one it set aside.
     */
    static void Release()
    {
        BluetoothRequestHandler::_pairing.Reset();
        delete BluetoothRequestHandler::_instance;
        BluetoothRequestHandler::_instance = _savedInstance;
        BluetoothRequestHandler::_instantiated = _savedInstantiated;
        BluetoothRequestHandler::_messageDispatcher = _savedDispatcher;
        _savedInstance = nullptr;
        _savedDispatcher = nullptr;
    }

    /**
     * @brief Pass a request to the handler, as its task would.
     *
     * @param request Request, on the heap; it becomes the response.
     */
    static void Dispatch(Message *request)
    {
        BluetoothRequestHandler::_instance->DispatchRequest(request);
    }

    /**
     * @brief Pass a GAP event to the callback, as Bluedroid does.
     *
     * @param event Event.
     * @param param Its parameters.
     */
    static void GapEvent(const esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t &param)
    {
        BluetoothRequestHandler::GAPEventHandler(event, &param);
    }

    /**
     * @brief Pass a GATTS event to the callback, as Bluedroid does.
     *
     * @param event Event.
     * @param param Its parameters.
     */
    static void GattsEvent(const esp_gatts_cb_event_t event, esp_ble_gatts_cb_param_t &param)
    {
        BluetoothRequestHandler::GATTSProfileEventHandler(event, 0, &param);
    }

    /**
     * @brief The pairing state the handler keeps.
     */
    static BluetoothPairing &Pairing()
    {
        return (BluetoothRequestHandler::_pairing);
    }

    /**
     * @brief Take the handler away, as when it has been destroyed.
     *
     * @return The handler that was there, for Restore().
     */
    static BluetoothRequestHandler *Forget()
    {
        BluetoothRequestHandler * const previous = BluetoothRequestHandler::_instance;
        BluetoothRequestHandler::_instance = nullptr;
        return (previous);
    }

    /**
     * @brief Put back the handler Forget() took away.
     *
     * @param handler The handler.
     */
    static void Restore(BluetoothRequestHandler *handler)
    {
        BluetoothRequestHandler::_instance = handler;
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

BluetoothRequestHandler *TestBluetoothPairing::_savedInstance = nullptr;
bool TestBluetoothPairing::_savedInstantiated = false;
IMessageDispatcher *TestBluetoothPairing::_savedDispatcher = nullptr;

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
 * @brief Release any messages earlier tests left with the dispatcher.
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
 * @brief Make a handler of the test's own with an empty dispatcher.
 *
 * @return The dispatcher.
 */
static MockMessageDispatcher *Begin()
{
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    TestBluetoothPairing::Create(dispatcher);
    return (dispatcher);
}

/**
 * @brief Check nothing more was sent to the STM32 and put the handler back.
 *
 * @param dispatcher The dispatcher.
 */
static void End(MockMessageDispatcher *dispatcher)
{
    Message *left = dispatcher->GetOutboundMessage();
    if (left != nullptr)
    {
        Message::DeleteMessage(left);
        TestBluetoothPairing::Release();
        TEST_FAIL_MESSAGE("A message nobody expected was sent to the STM32.");
    }
    TestBluetoothPairing::Release();
}

/**
 * @brief Send the handler a request and return its response.
 *
 * @param dispatcher Dispatcher the response is queued on.
 * @param function Function requested.
 * @param bytes Payload, copied; nullptr for none.
 * @param length Length of the payload.
 *
 * @return The response, to be released by the caller.
 */
static Message *Request(MockMessageDispatcher *dispatcher, const uint32_t function, const uint8_t *bytes, const uint32_t length)
{
    uint8_t *payload = nullptr;
    if (length > 0)
    {
        payload = static_cast<uint8_t *>(pvPortMalloc(length));
        TEST_ASSERT_NOT_NULL(payload);
        memcpy(payload, bytes, length);
    }
    Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::BlueTooth, function, StatusCodes::Failure, 0, payload, length);
    TEST_ASSERT_NOT_NULL(request);
    TestBluetoothPairing::Dispatch(request);

    Message *response = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(response, "The request was not answered.");
    return (response);
}

/**
 * @brief Send a pairing reply and check the status it is answered with.
 *
 * @param dispatcher Dispatcher the response is queued on.
 * @param function PasskeyReply or NumericComparisonReply.
 * @param address Client the reply is about.
 * @param accept Whether the user accepted.
 * @param passkey Passkey the user entered.
 * @param expected Status the response must carry.
 * @param message What went wrong, if it does not.
 */
static void Reply(MockMessageDispatcher *dispatcher, const uint32_t function, const esp_bd_addr_t address, const bool accept, const uint32_t passkey, const uint32_t expected, const char *message)
{
    uint8_t payload[PAIRING_REPLY_LENGTH];
    memcpy(payload, address, Esp32Messaging::BT_ADDRESS_LENGTH);
    payload[Esp32Messaging::BT_ADDRESS_LENGTH] = accept ? 1 : 0;
    Encoders::EncodeUInt32(passkey, payload + Esp32Messaging::BT_ADDRESS_LENGTH + 1);

    Message *response = Request(dispatcher, function, payload, sizeof(payload));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, response->StatusCode, message);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, function, expected);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);
}

/**
 * @brief Report a numeric comparison, as Bluedroid does.
 *
 * @param address Client being paired with.
 * @param number Number both sides show.
 */
static void NumericComparisonEvent(const esp_bd_addr_t address, const uint32_t number)
{
    esp_ble_gap_cb_param_t param = {};
    memcpy(param.ble_security.key_notif.bd_addr, address, sizeof(esp_bd_addr_t));
    param.ble_security.key_notif.passkey = number;
    TestBluetoothPairing::GapEvent(ESP_GAP_BLE_NC_REQ_EVT, param);
}

/**
 * @brief Report a request for the passkey the client shows, as Bluedroid does.
 *
 * @param address Client being paired with.
 */
static void PasskeyRequestEvent(const esp_bd_addr_t address)
{
    esp_ble_gap_cb_param_t param = {};
    memcpy(param.ble_security.ble_req.bd_addr, address, sizeof(esp_bd_addr_t));
    TestBluetoothPairing::GapEvent(ESP_GAP_BLE_PASSKEY_REQ_EVT, param);
}

/**
 * @brief Report the end of pairing, as Bluedroid does.
 *
 * @param address Client paired with.
 * @param success Whether it succeeded.
 * @param reason Why it failed.
 * @param mode ESP_LE_AUTH_* bits achieved.
 */
static void AuthenticationCompleteEvent(const esp_bd_addr_t address, const bool success, const esp_ble_auth_fail_rsn_t reason, const esp_ble_auth_req_t mode)
{
    esp_ble_gap_cb_param_t param = {};
    memcpy(param.ble_security.auth_cmpl.bd_addr, address, sizeof(esp_bd_addr_t));
    param.ble_security.auth_cmpl.success = success;
    param.ble_security.auth_cmpl.fail_reason = reason;
    param.ble_security.auth_cmpl.addr_type = BLE_ADDR_TYPE_RANDOM;
    param.ble_security.auth_cmpl.auth_mode = mode;
    TestBluetoothPairing::GapEvent(ESP_GAP_BLE_AUTH_CMPL_EVT, param);
}

/**
 * @brief Take the next event, check it is a pairing event about the client, and release it.
 *
 * @param dispatcher Dispatcher holding the event.
 * @param function Event expected.
 * @param passkey Passkey it must carry.
 */
static void CheckPairingEvent(MockMessageDispatcher *dispatcher, const uint32_t function, const uint32_t passkey)
{
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "The STM32 was not told.");
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, function, StatusCodes::CompletedOk);
    TEST_ASSERT_EQUAL_UINT32(PAIRING_EVENT_LENGTH, event->PayloadLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(CLIENT, event->Payload, Esp32Messaging::BT_ADDRESS_LENGTH, "The event names the wrong client.");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(passkey, Encoders::ExtractUInt32(event->Payload + Esp32Messaging::BT_ADDRESS_LENGTH), "The event carries the wrong number.");
    Message::DeleteMessage(event);
}

/**
 * @brief Check that the stack is down, as the tests that rely on its refusals need.
 */
static void CheckStackIsDown()
{
    TEST_ASSERT_EQUAL_MESSAGE(ESP_BLUEDROID_STATUS_UNINITIALIZED, esp_bluedroid_get_status(), "These tests need the Bluetooth stack down; an earlier test left it running.");
}

/**
 * @brief A numeric comparison is put to the user through the STM32 rather than confirmed.
 *
 *  The comparison used to be confirmed without asking anyone, which gave none of the protection
 *  it exists for (issue #398, point 4).
 */
TEST_CASE("A numeric comparison is passed to the STM32 rather than confirmed", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Begin();

    NumericComparisonEvent(CLIENT, PASSKEY);
    CheckPairingEvent(dispatcher, BluetoothFunction::NumericComparisonRequestEvent, PASSKEY);
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::NumericComparison), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "No reply is expected from the STM32.");

    End(dispatcher);
}

TEST_CASE("A request for the client's passkey is passed to the STM32", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Begin();

    PasskeyRequestEvent(CLIENT);
    CheckPairingEvent(dispatcher, BluetoothFunction::PasskeyRequestEvent, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(BluetoothPairing::PendingReply::Passkey), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()));

    End(dispatcher);
}

TEST_CASE("A passkey to show the user is passed to the STM32", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Begin();

    esp_ble_gap_cb_param_t param = {};
    memcpy(param.ble_security.key_notif.bd_addr, CLIENT, sizeof(esp_bd_addr_t));
    param.ble_security.key_notif.passkey = PASSKEY;
    TestBluetoothPairing::GapEvent(ESP_GAP_BLE_PASSKEY_NOTIF_EVT, param);
    CheckPairingEvent(dispatcher, BluetoothFunction::PasskeyDisplayEvent, PASSKEY);
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::None), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "Showing a passkey needs no reply.");

    End(dispatcher);
}

TEST_CASE("A pairing reply nobody asked for is refused", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Begin();

    Reply(dispatcher, BluetoothFunction::NumericComparisonReply, CLIENT, true, 0, StatusCodes::InvalidRequest, "A confirmation nobody asked for was accepted.");
    Reply(dispatcher, BluetoothFunction::PasskeyReply, CLIENT, true, PASSKEY, StatusCodes::InvalidRequest, "A passkey nobody asked for was accepted.");

    End(dispatcher);
}

/**
 * @brief Only the answer to the question asked, about the client it was asked about, is accepted.
 *
 *  A wrong reply leaves the question open.  The right one is passed to the stack, which is down
 *  here, so it is answered BluetoothNotStarted, and the question is closed.
 */
TEST_CASE("A pairing reply about another client, or to another question, is refused", CLASS_OF_TESTS)
{
    CheckStackIsDown();
    MockMessageDispatcher *dispatcher = Begin();

    NumericComparisonEvent(CLIENT, PASSKEY);
    CheckPairingEvent(dispatcher, BluetoothFunction::NumericComparisonRequestEvent, PASSKEY);

    Reply(dispatcher, BluetoothFunction::NumericComparisonReply, OTHER_CLIENT, true, 0, StatusCodes::InvalidRequest, "A confirmation for another client was accepted.");
    Reply(dispatcher, BluetoothFunction::PasskeyReply, CLIENT, true, PASSKEY, StatusCodes::InvalidRequest, "A passkey was accepted for a numeric comparison.");
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::NumericComparison), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "A wrong reply closed the question.");

    Reply(dispatcher, BluetoothFunction::NumericComparisonReply, CLIENT, true, 0, StatusCodes::BluetoothNotStarted, "The right reply did not reach the stack.");
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::None), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "The question stayed open after it was answered.");
    Reply(dispatcher, BluetoothFunction::NumericComparisonReply, CLIENT, true, 0, StatusCodes::InvalidRequest, "A question was answered twice.");

    End(dispatcher);
}

TEST_CASE("A passkey of more than six digits is refused and the question stays open", CLASS_OF_TESTS)
{
    CheckStackIsDown();
    MockMessageDispatcher *dispatcher = Begin();

    PasskeyRequestEvent(CLIENT);
    CheckPairingEvent(dispatcher, BluetoothFunction::PasskeyRequestEvent, 0);

    Reply(dispatcher, BluetoothFunction::PasskeyReply, CLIENT, true, 1000000, StatusCodes::InvalidRequest, "A seven digit passkey was accepted.");
    TEST_ASSERT_EQUAL(static_cast<int>(BluetoothPairing::PendingReply::Passkey), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()));

    //
    //  Refusing the pairing needs no passkey, so whatever passkey comes with it is not checked.
    //
    Reply(dispatcher, BluetoothFunction::PasskeyReply, CLIENT, false, 1000000, StatusCodes::BluetoothNotStarted, "Refusing the pairing did not reach the stack.");
    TEST_ASSERT_EQUAL(static_cast<int>(BluetoothPairing::PendingReply::None), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()));

    End(dispatcher);
}

TEST_CASE("A malformed pairing reply is refused", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Begin();

    NumericComparisonEvent(CLIENT, PASSKEY);
    CheckPairingEvent(dispatcher, BluetoothFunction::NumericComparisonRequestEvent, PASSKEY);

    for (const uint32_t function: {static_cast<uint32_t>(BluetoothFunction::PasskeyReply), static_cast<uint32_t>(BluetoothFunction::NumericComparisonReply)})
    {
        Message *response = Request(dispatcher, function, nullptr, 0);
        ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, function, StatusCodes::InvalidRequest);
        Message::DeleteMessage(response);

        uint8_t shortPayload[PAIRING_REPLY_LENGTH - 1];
        memcpy(shortPayload, CLIENT, Esp32Messaging::BT_ADDRESS_LENGTH);
        memset(shortPayload + Esp32Messaging::BT_ADDRESS_LENGTH, 1, sizeof(shortPayload) - Esp32Messaging::BT_ADDRESS_LENGTH);
        response = Request(dispatcher, function, shortPayload, sizeof(shortPayload));
        ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, function, StatusCodes::InvalidRequest);
        Message::DeleteMessage(response);
    }
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::NumericComparison), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "A malformed reply closed the question.");

    End(dispatcher);
}

/**
 * @brief The bonds are kept by the stack, so asking about them while it is down is answered
 *        BluetoothNotStarted rather than with an empty list.
 */
TEST_CASE("Bond requests are refused while the Bluetooth stack is not running", CLASS_OF_TESTS)
{
    CheckStackIsDown();
    MockMessageDispatcher *dispatcher = Begin();

    Message *response = Request(dispatcher, BluetoothFunction::GetBondedDevices, nullptr, 0);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::GetBondedDevices, StatusCodes::BluetoothNotStarted);
    ASSERT_NO_PAYLOAD(response);
    Message::DeleteMessage(response);

    response = Request(dispatcher, BluetoothFunction::RemoveAllBonds, nullptr, 0);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::RemoveAllBonds, StatusCodes::BluetoothNotStarted);
    Message::DeleteMessage(response);

    response = Request(dispatcher, BluetoothFunction::RemoveBond, CLIENT, sizeof(esp_bd_addr_t));
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::RemoveBond, StatusCodes::BluetoothNotStarted);
    Message::DeleteMessage(response);

    End(dispatcher);
}

TEST_CASE("A RemoveBond request without a whole address is refused", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Begin();

    Message *response = Request(dispatcher, BluetoothFunction::RemoveBond, nullptr, 0);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::RemoveBond, StatusCodes::InvalidRequest);
    Message::DeleteMessage(response);

    response = Request(dispatcher, BluetoothFunction::RemoveBond, CLIENT, sizeof(esp_bd_addr_t) - 1);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::RemoveBond, StatusCodes::InvalidRequest);
    Message::DeleteMessage(response);

    End(dispatcher);
}

TEST_CASE("A failed pairing is reported to the STM32 and closes the question", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Begin();

    NumericComparisonEvent(CLIENT, PASSKEY);
    CheckPairingEvent(dispatcher, BluetoothFunction::NumericComparisonRequestEvent, PASSKEY);

    AuthenticationCompleteEvent(CLIENT, false, ESP_AUTH_SMP_CONFIRM_VALUE_FAIL, 0);
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "The failure was not reported.");
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::AuthenticationCompleteEvent, StatusCodes::Failure);
    TEST_ASSERT_EQUAL_UINT32(AUTHENTICATION_EVENT_LENGTH, event->PayloadLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(CLIENT, event->Payload, Esp32Messaging::BT_ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(BLE_ADDR_TYPE_RANDOM, event->Payload[6], "Wrong address type.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, event->Payload[7], "Wrong authentication mode.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(ESP_AUTH_SMP_CONFIRM_VALUE_FAIL, event->Payload[8], "Wrong failure reason.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, event->Payload[9], "A failed pairing is reported as bonded.");
    Message::DeleteMessage(event);

    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::None), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "The question stayed open after pairing ended.");

    End(dispatcher);
}

/**
 * @brief A pairing that succeeds is reported; the client is bonded only if the stack lists it.
 *
 *  The stack is down here, so its bond list is empty and the client is not treated as bonded.
 */
TEST_CASE("A successful pairing is reported to the STM32", CLASS_OF_TESTS)
{
    CheckStackIsDown();
    MockMessageDispatcher *dispatcher = Begin();

    AuthenticationCompleteEvent(CLIENT, true, ESP_AUTH_SMP_PASSKEY_FAIL, ESP_LE_AUTH_REQ_SC_MITM_BOND);
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "The pairing was not reported.");
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::AuthenticationCompleteEvent, StatusCodes::CompletedOk);
    TEST_ASSERT_EQUAL_UINT32(AUTHENTICATION_EVENT_LENGTH, event->PayloadLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(CLIENT, event->Payload, Esp32Messaging::BT_ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_UINT8(ESP_LE_AUTH_REQ_SC_MITM_BOND, event->Payload[7]);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, event->Payload[8], "A failure reason was reported for a pairing that succeeded.");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, event->Payload[9], "A client the stack has not listed is reported as bonded.");
    Message::DeleteMessage(event);

    esp_bd_addr_t bonded;
    TEST_ASSERT_FALSE_MESSAGE(TestBluetoothPairing::Pairing().GetBondedClient(bonded), "A client the stack has not listed is treated as bonded.");

    End(dispatcher);
}

/**
 * @brief Nothing about one connection's pairing carries over to the next.
 */
TEST_CASE("A client connecting or disconnecting closes the question and forgets the bonded client", CLASS_OF_TESTS)
{
    CheckStackIsDown();
    MockMessageDispatcher *dispatcher = Begin();

    NumericComparisonEvent(CLIENT, PASSKEY);
    CheckPairingEvent(dispatcher, BluetoothFunction::NumericComparisonRequestEvent, PASSKEY);
    TestBluetoothPairing::Pairing().SetBondedClient(OTHER_CLIENT);

    esp_ble_gatts_cb_param_t param = {};
    memcpy(param.connect.remote_bda, CLIENT, sizeof(esp_bd_addr_t));
    param.connect.conn_id = 0;
    TestBluetoothPairing::GattsEvent(ESP_GATTS_CONNECT_EVT, param);
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(event);
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientConnectedEvent, StatusCodes::CompletedOk);
    Message::DeleteMessage(event);
    esp_bd_addr_t bonded;
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::None), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "A question from before the connection stayed open.");
    TEST_ASSERT_FALSE_MESSAGE(TestBluetoothPairing::Pairing().GetBondedClient(bonded), "A new client was taken to be bonded before it encrypted.");

    NumericComparisonEvent(CLIENT, PASSKEY);
    CheckPairingEvent(dispatcher, BluetoothFunction::NumericComparisonRequestEvent, PASSKEY);
    TestBluetoothPairing::Pairing().SetBondedClient(CLIENT);

    param = {};
    param.disconnect.conn_id = 0;
    TestBluetoothPairing::GattsEvent(ESP_GATTS_DISCONNECT_EVT, param);
    event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL(event);
    ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
    Message::DeleteMessage(event);
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::None), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "A question stayed open after the client left.");
    TEST_ASSERT_FALSE_MESSAGE(TestBluetoothPairing::Pairing().GetBondedClient(bonded), "The bonded client was remembered after it left.");

    End(dispatcher);
}

TEST_CASE("Pairing events that arrive with no handler are ignored", CLASS_OF_TESTS)
{
    MockMessageDispatcher *dispatcher = Begin();
    BluetoothRequestHandler * const handler = TestBluetoothPairing::Forget();

    NumericComparisonEvent(CLIENT, PASSKEY);
    PasskeyRequestEvent(CLIENT);
    AuthenticationCompleteEvent(CLIENT, false, ESP_AUTH_SMP_PAIR_AUTH_FAIL, 0);
    esp_ble_gap_cb_param_t param = {};
    memcpy(param.ble_security.ble_req.bd_addr, CLIENT, sizeof(esp_bd_addr_t));
    TestBluetoothPairing::GapEvent(ESP_GAP_BLE_SEC_REQ_EVT, param);
    TestBluetoothPairing::GapEvent(ESP_GAP_BLE_PASSKEY_NOTIF_EVT, param);
    TestBluetoothPairing::GapEvent(ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT, param);

    TestBluetoothPairing::Restore(handler);
    TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(BluetoothPairing::PendingReply::None), static_cast<int>(TestBluetoothPairing::Pairing().GetPendingReply()), "An event with no handler opened a question.");
    End(dispatcher);
}

TEST_CASE("BluetoothPairing accepts only the reply that is due", CLASS_OF_TESTS)
{
    BluetoothPairing pairing;
    TEST_ASSERT_FALSE_MESSAGE(pairing.Take(BluetoothPairing::PendingReply::Passkey, CLIENT), "A reply was taken when none was due.");
    TEST_ASSERT_FALSE_MESSAGE(pairing.Take(BluetoothPairing::PendingReply::None, CLIENT), "PendingReply::None was taken as a reply.");

    pairing.Expect(BluetoothPairing::PendingReply::Passkey, CLIENT);
    TEST_ASSERT_FALSE(pairing.Take(BluetoothPairing::PendingReply::NumericComparison, CLIENT));
    TEST_ASSERT_FALSE(pairing.Take(BluetoothPairing::PendingReply::Passkey, OTHER_CLIENT));
    TEST_ASSERT_TRUE(pairing.Take(BluetoothPairing::PendingReply::Passkey, CLIENT));
    TEST_ASSERT_EQUAL(static_cast<int>(BluetoothPairing::PendingReply::None), static_cast<int>(pairing.GetPendingReply()));
    TEST_ASSERT_FALSE_MESSAGE(pairing.Take(BluetoothPairing::PendingReply::Passkey, CLIENT), "A reply was taken twice.");

    //
    //  A new question replaces one still open: the stack pairs with one client at a time.
    //
    pairing.Expect(BluetoothPairing::PendingReply::Passkey, CLIENT);
    pairing.Expect(BluetoothPairing::PendingReply::NumericComparison, OTHER_CLIENT);
    TEST_ASSERT_FALSE(pairing.Take(BluetoothPairing::PendingReply::Passkey, CLIENT));
    pairing.ClearPendingReply();
    TEST_ASSERT_FALSE(pairing.Take(BluetoothPairing::PendingReply::NumericComparison, OTHER_CLIENT));
}

/**
 * @brief A bonded client's subscriptions last until it reconnects (Bluetooth Core Specification,
 *        Vol 3, Part G, 3.3.3.3), one set for each client.
 */
TEST_CASE("BluetoothPairing keeps each bonded client's subscriptions", CLASS_OF_TESTS)
{
    BluetoothPairing pairing;
    const std::vector<BluetoothPairing::ClientConfiguration> configurations = {{0x002b, 0x0001}, {0x002e, 0x0000}, {0x0031, 0x0002}};
    pairing.SaveClientConfigurations(CLIENT, configurations);

    std::vector<BluetoothPairing::ClientConfiguration> kept = pairing.GetClientConfigurations(CLIENT);
    TEST_ASSERT_EQUAL_MESSAGE(2, kept.size(), "Only the subscriptions that are on should be kept.");
    TEST_ASSERT_EQUAL_HEX16(0x002b, kept[0].Handle);
    TEST_ASSERT_EQUAL_HEX16(0x0001, kept[0].Value);
    TEST_ASSERT_EQUAL_HEX16(0x0031, kept[1].Handle);
    TEST_ASSERT_EQUAL_HEX16(0x0002, kept[1].Value);
    TEST_ASSERT_EQUAL_MESSAGE(0, pairing.GetClientConfigurations(OTHER_CLIENT).size(), "One client was given another's subscriptions.");

    //
    //  A client that leaves with nothing switched on has nothing kept.
    //
    pairing.SaveClientConfigurations(CLIENT, {{0x002b, 0x0000}});
    TEST_ASSERT_EQUAL(0, pairing.GetClientConfigurations(CLIENT).size());

    pairing.SaveClientConfigurations(CLIENT, configurations);
    pairing.SaveClientConfigurations(OTHER_CLIENT, configurations);
    pairing.ForgetClient(OTHER_CLIENT);
    TEST_ASSERT_EQUAL_MESSAGE(0, pairing.GetClientConfigurations(OTHER_CLIENT).size(), "A client whose bond was removed kept its subscriptions.");
    TEST_ASSERT_EQUAL_MESSAGE(2, pairing.GetClientConfigurations(CLIENT).size(), "Forgetting one client forgot another.");

    pairing.Reset();
    TEST_ASSERT_EQUAL_MESSAGE(0, pairing.GetClientConfigurations(CLIENT).size(), "Subscriptions outlived a reset.");
}

TEST_CASE("BluetoothPairing stops treating a client as bonded when its bond is removed", CLASS_OF_TESTS)
{
    BluetoothPairing pairing;
    esp_bd_addr_t bonded = {};
    TEST_ASSERT_FALSE(pairing.GetBondedClient(bonded));

    pairing.SetBondedClient(CLIENT);
    pairing.ForgetClient(OTHER_CLIENT);
    TEST_ASSERT_TRUE_MESSAGE(pairing.GetBondedClient(bonded), "Removing another client's bond affected this one.");
    TEST_ASSERT_EQUAL_UINT8_ARRAY(CLIENT, bonded, sizeof(esp_bd_addr_t));

    pairing.ForgetClient(CLIENT);
    TEST_ASSERT_FALSE_MESSAGE(pairing.GetBondedClient(bonded), "A client whose bond was removed is still treated as bonded.");
}

TEST_CASE("Pairing payloads are encoded as documented", "[Encoders][Emulated]")
{
    Esp32Messaging::BTPairingEventData pairingEvent;
    memcpy(pairingEvent.Address, CLIENT, sizeof(pairingEvent.Address));
    pairingEvent.Passkey = 0x000f1206;
    TEST_ASSERT_EQUAL(PAIRING_EVENT_LENGTH, Encoders::EncodedBTPairingEventDataBufferSize(&pairingEvent));
    uint8_t buffer[32];
    memset(buffer, 0xee, sizeof(buffer));
    Encoders::EncodeBTPairingEventData(&pairingEvent, buffer);
    const uint8_t expectedPairingEvent[] = {0x34, 0x5f, 0x45, 0x01, 0x02, 0x03, 0x06, 0x12, 0x0f, 0x00, 0xee};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedPairingEvent, buffer, sizeof(expectedPairingEvent));

    Esp32Messaging::BTAuthenticationCompleteEventData complete;
    memcpy(complete.Address, CLIENT, sizeof(complete.Address));
    complete.AddressType = 1;
    complete.AuthenticationMode = 0x0d;
    complete.FailureReason = 0x63;
    complete.Bonded = 1;
    TEST_ASSERT_EQUAL(AUTHENTICATION_EVENT_LENGTH, Encoders::EncodedBTAuthenticationCompleteEventDataBufferSize(&complete));
    memset(buffer, 0xee, sizeof(buffer));
    Encoders::EncodeBTAuthenticationCompleteEventData(&complete, buffer);
    const uint8_t expectedComplete[] = {0x34, 0x5f, 0x45, 0x01, 0x02, 0x03, 0x01, 0x0d, 0x63, 0x01, 0xee};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedComplete, buffer, sizeof(expectedComplete));

    uint8_t devices[2 * Esp32Messaging::BT_BONDED_DEVICE_ENTRY_LENGTH] = {0x34, 0x5f, 0x45, 0x01, 0x02, 0x03, 0x00, 0x34, 0x5f, 0x45, 0x0a, 0x0b, 0x0c, 0x01};
    Esp32Messaging::BTBondedDevicesResponse bonded;
    bonded.DeviceCount = 2;
    bonded.DevicesLength = sizeof(devices);
    bonded.Devices = devices;
    TEST_ASSERT_EQUAL(6 + sizeof(devices), Encoders::EncodedBTBondedDevicesResponseBufferSize(&bonded));
    memset(buffer, 0xee, sizeof(buffer));
    Encoders::EncodeBTBondedDevicesResponse(&bonded, buffer);
    const uint8_t expectedHeader[] = {0x02, 0x00, 0x0e, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedHeader, buffer, sizeof(expectedHeader));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(devices, buffer + sizeof(expectedHeader), sizeof(devices));
    TEST_ASSERT_EQUAL_HEX8(0xee, buffer[sizeof(expectedHeader) + sizeof(devices)]);

    bonded.DeviceCount = 0;
    bonded.DevicesLength = 0;
    bonded.Devices = nullptr;
    TEST_ASSERT_EQUAL(6, Encoders::EncodedBTBondedDevicesResponseBufferSize(&bonded));
}

TEST_CASE("Pairing requests are decoded, and short ones refused as malformed", "[Encoders][Emulated]")
{
    const uint8_t reply[PAIRING_REPLY_LENGTH] = {0x34, 0x5f, 0x45, 0x01, 0x02, 0x03, 0x01, 0x40, 0xe2, 0x01, 0x00};
    bool malformed = true;
    Esp32Messaging::BTPairingReply *decoded = Encoders::ExtractBTPairingReply(reply, sizeof(reply), &malformed);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_FALSE(malformed);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(CLIENT, decoded->Address, sizeof(decoded->Address));
    TEST_ASSERT_EQUAL_UINT8(1, decoded->Accept);
    TEST_ASSERT_EQUAL_UINT32(123456, decoded->Passkey);
    Utils::DeleteAndNullifyPointer(decoded);

    for (uint32_t length = 0; length < sizeof(reply); length++)
    {
        malformed = false;
        TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractBTPairingReply(reply, length, &malformed), "A short pairing reply was decoded.");
        TEST_ASSERT_TRUE(malformed);
    }

    Esp32Messaging::BTDeviceAddress *address = Encoders::ExtractBTDeviceAddress(CLIENT, sizeof(esp_bd_addr_t), &malformed);
    TEST_ASSERT_NOT_NULL(address);
    TEST_ASSERT_FALSE(malformed);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(CLIENT, address->Address, sizeof(address->Address));
    Utils::DeleteAndNullifyPointer(address);

    for (uint32_t length = 0; length < sizeof(esp_bd_addr_t); length++)
    {
        malformed = false;
        TEST_ASSERT_NULL_MESSAGE(Encoders::ExtractBTDeviceAddress(CLIENT, length, &malformed), "A short address was decoded.");
        TEST_ASSERT_TRUE(malformed);
    }
}
