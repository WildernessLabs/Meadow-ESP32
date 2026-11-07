/*
 *  TestBluetoothHandles.cpp
 *
 *  Tests that GetHandles reports only the handles the service graph has (issue #636).
 */

#include "sdkconfig.h"

#include <new>
#include <stdint.h>
#include <string.h>
#include "unity.h"

#include "freertos/FreeRTOS.h"

#include "BluetoothRequestHandler.hpp"
#include "Encoders.hpp"
#include "MockMessageDispatcher.hpp"
#include "TestAssertions.hpp"
#include "TestEncoders.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Bluetooth][Emulated]"

/**
 * @brief Length of the header in a GetHandles payload: the count, then the length of the list in bytes.
 */
#define HANDLES_PAYLOAD_HEADER_LENGTH (sizeof(uint16_t) + sizeof(uint32_t))

/**
 * @brief Byte the memory a handler is built in is filled with before it is built.
 *
 *  It stands for whatever a previous user of that memory left there.  Repeated through
 *  _attributeTableLength it reads 0x0303, 771 handles: few enough that the buffer for them is
 *  allocated and sent, rather than refused, if the length is not initialised.
 */
#define LEFT_OVER_BYTE 0x03

/**
 * @brief Number of attributes in the table built from CONFIGURATION: the service, the characteristic's declaration and value, and its descriptor.
 */
#define CONFIGURATION_ATTRIBUTE_COUNT 4

/**
 * @brief Configuration of one service with one characteristic carrying one descriptor.
 */
static const char *CONFIGURATION = "{\"deviceName\":\"MeadowTest\",\"services\":[{\"name\":\"S\",\"uuid\":254,\"characteristics\":[{\"uuid\":10755,\"permission\":17,\"props\":14,"
                                   "\"len\":4,\"descriptors\":[{\"uuid\":10498,\"permission\":17,\"len\":2,\"value\":0}]}]}]}";

/**
 * @brief Configuration the manager parses and then refuses: its characteristic needs MITM protection the security settings do not ask for.
 *
 *  A configuration with no services would not do: the manager adds to the graph it already
 *  has, so it is not refused when that graph holds a service.
 */
static const char *REFUSED_CONFIGURATION = "{\"deviceName\":\"MeadowTest\",\"services\":[{\"name\":\"M\",\"uuid\":248,\"characteristics\":[{\"uuid\":10755,\"permission\":68,\"props\":10,\"len\":4}]}]}";

/**
 * @brief Gives the tests access to BluetoothRequestHandler's attribute table and GetHandles.
 */
class TestBluetoothHandles
{
public:
    /**
     * @brief Make a handler of the test's own in memory that already holds LEFT_OVER_BYTE throughout.
     *
     *  The handler is created without Setup(), so it starts no task and no Bluetooth stack.
     *  Any handler the test dispatcher already holds is set aside and put back by Release().
     *  Building the handler over memory filled with a known value, rather than relying on
     *  what a fresh heap holds, makes a member that the constructor leaves alone show up.
     *
     * @param dispatcher Dispatcher the handler raises its events and responses through.
     *
     * @return The handler.
     */
    static BluetoothRequestHandler *Create(MockMessageDispatcher *dispatcher)
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

        //
        //  The service graph lives in the manager, which every handler shares, so whatever an
        //  earlier test left in it goes before the handler is built.
        //
        BluetoothRequestHandler::_manager.DeleteAllServices();

        _memory = ::operator new(sizeof(BluetoothRequestHandler));
        TEST_ASSERT_NOT_NULL(_memory);
        memset(_memory, LEFT_OVER_BYTE, sizeof(BluetoothRequestHandler));
        BluetoothRequestHandler::_instance = new (_memory) BluetoothRequestHandler(dispatcher);
        return (BluetoothRequestHandler::_instance);
    }

    /**
     * @brief Destroy the handler made by Create(), empty the service graph and put back the handler it set aside.
     */
    static void Release()
    {
        BluetoothRequestHandler::_instance->~BluetoothRequestHandler();
        ::operator delete(_memory);
        _memory = nullptr;
        BluetoothRequestHandler::_manager.DeleteAllServices();
        BluetoothRequestHandler::_instance = _savedInstance;
        BluetoothRequestHandler::_instantiated = _savedInstantiated;
        BluetoothRequestHandler::_messageDispatcher = _savedDispatcher;
        _savedInstance = nullptr;
        _savedDispatcher = nullptr;
    }

    /**
     * @brief Build the service graph from CONFIGURATION and the attribute table from it, as a Start does.
     *
     *  Nothing more of the Start is done: the stack is not started, so this is what a Start
     *  whose stack then failed to come up leaves behind.
     *
     * @param handler Handler to build the table in.
     */
    static void BuildAttributeTable(BluetoothRequestHandler *handler)
    {
        TEST_ASSERT_TRUE_MESSAGE(BluetoothRequestHandler::_manager.BuildGraphFromJson(CONFIGURATION), "The configuration was not parsed.");
        handler->GenerateAttributeTable();
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(CONFIGURATION_ATTRIBUTE_COUNT, handler->GetAttributeTableLength(), "The attribute table does not have one entry for each attribute.");
        TEST_ASSERT_NOT_NULL(handler->GetAttributeTable());
    }

    /**
     * @brief Whether the handler holds an attribute table.
     *
     * @param handler Handler to look in.
     *
     * @return true if the handler has an attribute table, false if it has none.
     */
    static bool HasAttributeTable(BluetoothRequestHandler *handler)
    {
        return (handler->GetAttributeTable() != nullptr);
    }

    /**
     * @brief Call the private GetHandles method and return the response it queues.
     *
     * @param handler Handler to ask.
     * @param dispatcher Dispatcher the handler queues its response on.
     *
     * @return The response, which the caller releases.
     */
    static Message *GetHandles(BluetoothRequestHandler *handler, MockMessageDispatcher *dispatcher)
    {
        //
        //  The request has to be on the heap: the handler turns it into the response.
        //
        Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::BlueTooth, BluetoothFunction::GetHandles, StatusCodes::Failure);
        TEST_ASSERT_NOT_NULL(request);
        handler->GetHandles(request);

        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL_MESSAGE(response, "GetHandles was not answered.");
        ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::GetHandles, StatusCodes::CompletedOk);
        return (response);
    }

    /**
     * @brief Call the private Start method with a configuration and return the response it queues.
     *
     *  The BluetoothStartingEvent raised first is checked and released.
     *
     * @param handler Handler to start.
     * @param dispatcher Dispatcher the handler queues its events and response on.
     * @param configuration Configuration to start with.
     *
     * @return The response, which the caller releases.
     */
    static Message *Start(BluetoothRequestHandler *handler, MockMessageDispatcher *dispatcher, const char * const configuration)
    {
        Esp32Messaging::BTStackConfig stackConfig = {};
        stackConfig.Config = const_cast<char *>(configuration);
        const uint32_t payloadLength = TestEncoders::EncodedBTStackConfigBufferSize(&stackConfig);
        uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        TEST_ASSERT_NOT_NULL(payload);
        TestEncoders::EncodeBTStackConfig(&stackConfig, payload);

        Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::BlueTooth, BluetoothFunction::Start, StatusCodes::Failure, 0, payload, payloadLength);
        TEST_ASSERT_NOT_NULL(request);
        handler->Start(request);

        Message *event = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL_MESSAGE(event, "Start raised no starting event.");
        ASSERT_EVENT(event, Esp32Interfaces::BlueTooth, BluetoothFunction::BluetoothStartingEvent, StatusCodes::CompletedOk);
        Message::DeleteMessage(event);

        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL_MESSAGE(response, "Start was not answered.");
        return (response);
    }

private:
    /**
     * @brief Memory the handler made by Create() is built in.
     */
    static void *_memory;

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

void *TestBluetoothHandles::_memory = nullptr;
BluetoothRequestHandler *TestBluetoothHandles::_savedInstance = nullptr;
bool TestBluetoothHandles::_savedInstantiated = false;
IMessageDispatcher *TestBluetoothHandles::_savedDispatcher = nullptr;

/**
 * @brief The dispatcher these tests use.  It lives for the whole run.
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
 * @brief Check a GetHandles response reports no handles, and release it.
 *
 * @param response Response to check.
 */
static void CheckNoHandles(Message *response)
{
    ASSERT_HAS_PAYLOAD(response);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, Encoders::ExtractUInt16(response->Payload), "GetHandles reported handles the service graph does not have.");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, Encoders::ExtractUInt32(response->Payload + sizeof(uint16_t)), "GetHandles sent a list of handles the service graph does not have.");
    TEST_ASSERT_EQUAL_UINT32(HANDLES_PAYLOAD_HEADER_LENGTH, response->PayloadLength);
    Message::DeleteMessage(response);
}

TEST_CASE("GetHandles reports no handles before Bluetooth has been started", CLASS_OF_TESTS)
{
    //
    //  The attribute table length was set only when a Start built the table, so before one it
    //  was whatever the handler's memory held, and GetHandles sent that many handles of
    //  uninitialised memory (issue #636).
    //
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    BluetoothRequestHandler *handler = TestBluetoothHandles::Create(dispatcher);

    CheckNoHandles(TestBluetoothHandles::GetHandles(handler, dispatcher));

    TestBluetoothHandles::Release();
}

TEST_CASE("GetHandles reports no handles once a refused configuration has deleted the service graph", CLASS_OF_TESTS)
{
    //
    //  A refused configuration deletes the service graph, but the attribute table built from
    //  the previous one, and its length, were kept, so GetHandles went on reporting that many
    //  handles of uninitialised memory (issue #636).  A Start whose stack did not come up
    //  leaves the graph and the table behind, which BuildAttributeTable stands in for.
    //
    MockMessageDispatcher *dispatcher = Dispatcher();
    DrainDispatcher(dispatcher);
    BluetoothRequestHandler *handler = TestBluetoothHandles::Create(dispatcher);

    TestBluetoothHandles::BuildAttributeTable(handler);
    Message *response = TestBluetoothHandles::GetHandles(handler, dispatcher);
    ASSERT_HAS_PAYLOAD(response);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(CONFIGURATION_ATTRIBUTE_COUNT, Encoders::ExtractUInt16(response->Payload), "GetHandles did not report the configured attributes.");
    Message::DeleteMessage(response);

    response = TestBluetoothHandles::Start(handler, dispatcher, REFUSED_CONFIGURATION);
    ASSERT_RESPONSE(response, Esp32Interfaces::BlueTooth, BluetoothFunction::Start, StatusCodes::InvalidConfigurationFile);
    Message::DeleteMessage(response);

    CheckNoHandles(TestBluetoothHandles::GetHandles(handler, dispatcher));
    TEST_ASSERT_FALSE_MESSAGE(TestBluetoothHandles::HasAttributeTable(handler), "The attribute table outlived the service graph it was built from.");

    TestBluetoothHandles::Release();
}
