/*
 *  TestWiFiEventState.cpp
 *
 *  Tests that the WiFi event state the event loop writes is never read torn (issue #287).
 */

#include "sdkconfig.h"

#include <atomic>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "MutexLock.hpp"
#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"

#include "TestAssertions.hpp"
#include "TestHarness.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[WiFiEventState][Emulated]"

/**
 * @brief Number of records the writer stores, and the reader reads.
 */
#define EVENT_STATE_ITERATIONS 20000

/**
 * @brief Milliseconds to wait for the writer and reader to finish.
 */
#define EVENT_STATE_TIMEOUT 60000

/**
 * @brief Gives the tests access to WiFiRequestHandler's private event state.
 */
class TestWiFiEventState
{
public:
    /**
     * @brief The handler the event state belongs to, creating one without starting the radio if none exists.
     *
     * @return The handler.
     */
    static WiFiRequestHandler *Handler()
    {
        if (WiFiRequestHandler::_instance == nullptr)
        {
            NvsManager::Setup();
            MockMessageDispatcher *dispatcher = new MockMessageDispatcher(nullptr);
            TEST_ASSERT_NOT_NULL(dispatcher);
            TEST_ASSERT_NOT_NULL(new WiFiRequestHandler(dispatcher));
        }
        return (WiFiRequestHandler::_instance);
    }

    /**
     * @brief Store an access point record the way StationConnectedEvent does, under the event state mutex.
     *
     * @param handler Handler to store it in.
     * @param record Record to store.
     */
    static void StoreAccessPoint(WiFiRequestHandler *handler, const wifi_event_sta_connected_t &record)
    {
        MutexLock lock(WiFiRequestHandler::EventStateMutex());
        handler->_apInformation = record;
    }

    /**
     * @brief Call the private PopulateConnectionEventDataStructure method.
     *
     * @param handler Handler to read from.
     * @param data Filled in with the connection details.
     */
    static void Populate(WiFiRequestHandler *handler, Esp32Messaging::ConnectEventData *data)
    {
        handler->PopulateConnectionEventDataStructure(data);
    }

    /**
     * @brief Restore an empty access point record.
     *
     * @param handler Handler to clear.
     */
    static void Clear(WiFiRequestHandler *handler)
    {
        const wifi_event_sta_connected_t empty = {};
        StoreAccessPoint(handler, empty);
    }

    /**
     * @brief The dispatcher the handler raises its events through, which is the mock Handler() made.
     *
     * @param handler Handler whose dispatcher is wanted.
     *
     * @return The dispatcher.
     */
    static MockMessageDispatcher *Dispatcher(WiFiRequestHandler *handler)
    {
        MockMessageDispatcher *dispatcher = static_cast<MockMessageDispatcher *>(handler->_messageDispatcher);
        TEST_ASSERT_NOT_NULL(dispatcher);
        return (dispatcher);
    }

    /**
     * @brief Give the handler an event group if it has none, as the access point events set bits in it.
     *
     *  Setup() makes the real one and starts a task, which these tests do not want.
     *
     * @return true if one was made here, so Release() has to delete it.
     */
    static bool GiveEventGroup()
    {
        if (WiFiRequestHandler::_xWiFiEventGroup != nullptr)
        {
            return (false);
        }
        WiFiRequestHandler::_xWiFiEventGroup = xEventGroupCreate();
        TEST_ASSERT_NOT_NULL(WiFiRequestHandler::_xWiFiEventGroup);
        return (true);
    }

    /**
     * @brief Delete the event group GiveEventGroup made.
     */
    static void ReleaseEventGroup()
    {
        vEventGroupDelete(WiFiRequestHandler::_xWiFiEventGroup);
        WiFiRequestHandler::_xWiFiEventGroup = nullptr;
    }

    /**
     * @brief Call the private AccessPointStartedEvent method.
     */
    static void AccessPointStarted(WiFiRequestHandler *handler)
    {
        handler->AccessPointStartedEvent();
    }

    /**
     * @brief Call the private AccessPointStoppedEvent method.
     */
    static void AccessPointStopped(WiFiRequestHandler *handler)
    {
        handler->AccessPointStoppedEvent();
    }

    /**
     * @brief Call the private NodeConnectedEvent method for a station.
     *
     * @param handler Handler to call it on.
     * @param mac Address of the station, the last byte of which is the only one that varies.
     */
    static void NodeConnected(WiFiRequestHandler *handler, const uint8_t lastByte)
    {
        ip_event_ap_staipassigned_t event = {};
        event.mac[0] = 0x02;
        event.mac[5] = lastByte;
        event.ip.addr = 0x0204a8c0;
        handler->NodeConnectedEvent(&event);
    }

    /**
     * @brief Fill the connected nodes map with stations that are not the one NodeConnected adds.
     *
     *  NodeConnected(handler, lastByte) finds or adds a station by the six bytes of its address.
     *  Filling the map with it already present means the map holds one fewer other station
     *  and adding it needs no allocation.
     *
     * @param handler Handler whose map is filled.
     * @param others Number of other stations to hold.
     * @param includeStation Last byte of the station NodeConnected will add, to hold already, or -1 for none.
     */
    static void FillConnectedNodes(WiFiRequestHandler *handler, const uint32_t others, const int includeStation)
    {
        handler->LockConnectedNodesMap();
        handler->_connectedNodes.clear();
        for (uint32_t index = 0; index < others; index++)
        {
            handler->_connectedNodes[0x1000 + index] = 1;
        }
        if (includeStation >= 0)
        {
            uint64_t key = 0;
            const uint8_t mac[6] = {0x02, 0, 0, 0, 0, static_cast<uint8_t>(includeStation)};
            memcpy(&key, mac, sizeof(mac));
            handler->_connectedNodes[key] = 1;
        }
        handler->UnlockConnectedNodesMap();
    }

    /**
     * @brief Forget every connected station.
     */
    static void ClearConnectedNodes(WiFiRequestHandler *handler)
    {
        handler->LockConnectedNodesMap();
        handler->_connectedNodes.clear();
        handler->UnlockConnectedNodesMap();
    }
};

/**
 * @brief State shared between the test and its writer and reader tasks.
 */
struct EventStateContext
{
    /**
     * @brief Handler whose event state is being written and read.
     */
    WiFiRequestHandler *Handler;

    /**
     * @brief The two records the writer alternates between.
     */
    wifi_event_sta_connected_t Records[2];

    /**
     * @brief Reads that matched neither record in full.
     */
    std::atomic<uint32_t> TornReads {0};

    /**
     * @brief Given once by each task when it has finished.
     */
    SemaphoreHandle_t Finished;

    /**
     * @brief Taken by each task before it starts, and given by the test once every task
     *        exists, so they all run at once.  Without it a task at a higher priority than
     *        the test runs to completion before the test creates the next one.
     */
    SemaphoreHandle_t Start;
};

/**
 * @brief Make a record whose ssid, ssid_len and channel all agree, so a mixture of two is detectable.
 *
 * @param fill Character the ssid is made of.
 * @param length Length of the ssid.
 * @param channel Channel recorded with it.
 * @return The record.
 */
static wifi_event_sta_connected_t MakeRecord(const char fill, const uint8_t length, const uint8_t channel)
{
    wifi_event_sta_connected_t record = {};
    memset(record.ssid, fill, length);
    record.ssid_len = length;
    memset(record.bssid, fill, sizeof(record.bssid));
    record.channel = channel;
    return (record);
}

/**
 * @brief Store the two records alternately, as fast as possible.
 *
 * @param parameter EventStateContext shared with the test.
 */
static void WriterTask(void *parameter)
{
    EventStateContext *context = static_cast<EventStateContext *>(parameter);
    xSemaphoreTake(context->Start, portMAX_DELAY);
    for (uint32_t iteration = 0; iteration < EVENT_STATE_ITERATIONS; iteration++)
    {
        TestWiFiEventState::StoreAccessPoint(context->Handler, context->Records[iteration % 2]);
    }
    xSemaphoreGive(context->Finished);
    vTaskDelete(nullptr);
}

/**
 * @brief Read the connection details as fast as possible, counting any that match neither record.
 *
 * @param parameter EventStateContext shared with the test.
 */
static void ReaderTask(void *parameter)
{
    EventStateContext *context = static_cast<EventStateContext *>(parameter);
    xSemaphoreTake(context->Start, portMAX_DELAY);
    for (uint32_t iteration = 0; iteration < EVENT_STATE_ITERATIONS; iteration++)
    {
        Esp32Messaging::ConnectEventData data = {};
        TestWiFiEventState::Populate(context->Handler, &data);
        bool whole = false;
        for (const wifi_event_sta_connected_t &record: context->Records)
        {
            if ((strlen(data.Ssid) == record.ssid_len) && (memcmp(data.Ssid, record.ssid, record.ssid_len) == 0) && (memcmp(data.Bssid, record.bssid, sizeof(data.Bssid)) == 0) && (data.Channel == record.channel))
            {
                whole = true;
            }
        }
        if (!whole)
        {
            context->TornReads.fetch_add(1);
        }
    }
    xSemaphoreGive(context->Finished);
    vTaskDelete(nullptr);
}

/**
 * @brief Check that connection details read while the event loop stores a new access point
 *        always come from one record, never part of one and part of another.
 *
 *  The event loop writes the access point details in StationConnectedEvent and the WiFi
 *  task reads them in PopulateConnectionEventDataStructure to answer a connect request.
 *  Neither held a lock, so the ssid, its length and the channel could come from two
 *  different connections (issue #287).  The writer here stores records the way
 *  StationConnectedEvent now does; StationConnectedEvent itself also sets the connected
 *  bit and raises an event, which would disturb later tests.
 */
TEST_CASE("Connection details are never read part way through an update", CLASS_OF_TESTS)
{
    //
    //  Static so a task that outlives a failed test cannot write to a dead stack frame.
    //
    static EventStateContext context;
    context.Handler = TestWiFiEventState::Handler();
    TEST_ASSERT_NOT_NULL(context.Handler);
    context.Records[0] = MakeRecord('A', 8, 1);
    context.Records[1] = MakeRecord('B', 24, 11);
    context.TornReads.store(0);
    context.Finished = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(context.Finished);
    context.Start = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(context.Start);
    TestWiFiEventState::StoreAccessPoint(context.Handler, context.Records[0]);

    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(WriterTask, "EventWriter", 4096, &context, tskIDLE_PRIORITY + 5, nullptr, 0));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(ReaderTask, "EventReader", 4096, &context, tskIDLE_PRIORITY + 5, nullptr, 1));
    xSemaphoreGive(context.Start);
    xSemaphoreGive(context.Start);
    for (uint32_t task = 0; task < 2; task++)
    {
        TEST_ASSERT_EQUAL_MESSAGE(pdTRUE, xSemaphoreTake(context.Finished, EVENT_STATE_TIMEOUT / portTICK_PERIOD_MS), "The writer or reader did not finish.");
    }

    printf("%" PRIu32 " torn reads in %" PRIu32 ".\n", context.TornReads.load(), static_cast<uint32_t>(EVENT_STATE_ITERATIONS));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, context.TornReads.load(), "Connection details were read part way through an update.");

    TestWiFiEventState::Clear(context.Handler);
    vSemaphoreDelete(context.Finished);
    vSemaphoreDelete(context.Start);
}

/**
 * @brief Release every message waiting for the STM32, so a test starts with an empty queue.
 *
 * @param dispatcher Dispatcher whose outbound queue is emptied.
 */
static void DrainOutbound(MockMessageDispatcher *dispatcher)
{
    while (dispatcher->OutboundMessageQueueLength() > 0)
    {
        Message *message = dispatcher->GetOutboundMessage();
        Message::DeleteMessage(message);
    }
}

/**
 * @brief Take the next message and check it is an event of the expected kind.
 *
 * @param dispatcher Dispatcher holding the message.
 * @param function Event expected.
 *
 * @return The event, which the caller releases.
 */
static Message *NextEvent(MockMessageDispatcher *dispatcher, const WiFiFunction::WiFiFunction function)
{
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, dispatcher->OutboundMessageQueueLength(), "The event was not queued.");
    Message *event = dispatcher->GetOutboundMessage();
    TEST_ASSERT_NOT_NULL_MESSAGE(event, "A null message was queued.");
    ASSERT_EVENT(event, Esp32Interfaces::WiFi, function, StatusCodes::CompletedOk);
    return (event);
}

/**
 * @brief Station used by the node tests, and the number of stations that fills the access point.
 */
#define TEST_STATION 0x77
#define MAXIMUM_STATIONS ESP_WIFI_MAX_CONN_NUM

/**
 * @brief Check the access point events still reach the STM32 as events.
 *
 *  AccessPointStartedEvent, AccessPointStoppedEvent, the node connection events and
 *  MaximumConnectionsReachedEvent build their own messages, and now raise them through
 *  RequestHandlerBase::RaiseEvent, which checks the message was made (issue #388).  This
 *  is the normal case, so the events must be unchanged.
 */
TEST_CASE("Access point events are queued for the STM32", CLASS_OF_TESTS)
{
    WiFiRequestHandler *handler = TestWiFiEventState::Handler();
    MockMessageDispatcher *dispatcher = TestWiFiEventState::Dispatcher(handler);
    const bool madeEventGroup = TestWiFiEventState::GiveEventGroup();
    DrainOutbound(dispatcher);

    TestWiFiEventState::AccessPointStopped(handler);
    Message *stopped = NextEvent(dispatcher, WiFiFunction::AccessPointStoppedEvent);
    ASSERT_NO_PAYLOAD(stopped);
    Message::DeleteMessage(stopped);

    //
    //  There is no access point interface here, so the started event has no address to carry.
    //
    TestWiFiEventState::AccessPointStarted(handler);
    Message *started = NextEvent(dispatcher, WiFiFunction::AccessPointStartedEvent);
    Message::DeleteMessage(started);

    TestWiFiEventState::ClearConnectedNodes(handler);
    TestWiFiEventState::NodeConnected(handler, TEST_STATION);
    Message *connected = NextEvent(dispatcher, WiFiFunction::NodeConnectedEvent);
    ASSERT_HAS_PAYLOAD(connected);
    Message::DeleteMessage(connected);
    TEST_ASSERT_EQUAL_MESSAGE(0, dispatcher->OutboundMessageQueueLength(), "An event was raised for a station that did not fill the access point.");

    TestWiFiEventState::FillConnectedNodes(handler, MAXIMUM_STATIONS - 1, -1);
    TestWiFiEventState::NodeConnected(handler, TEST_STATION);
    Message *last = NextEvent(dispatcher, WiFiFunction::NodeConnectedEvent);
    Message::DeleteMessage(last);
    Message *full = NextEvent(dispatcher, WiFiFunction::MaximumConnectionsReachedEvent);
    ASSERT_NO_PAYLOAD(full);
    Message::DeleteMessage(full);

    TestWiFiEventState::ClearConnectedNodes(handler);
    DrainOutbound(dispatcher);
    if (madeEventGroup)
    {
        TestWiFiEventState::ReleaseEventGroup();
    }
}

/**
 * @brief Check an access point event that cannot be made is dropped, not queued as a null message.
 *
 *  Message::CreateOnHeap returns nullptr when memory runs out.  The access point events
 *  passed that straight to QueueMessageForStm32, which queued a null message that held one of
 *  the outbound queue's slots for nothing, and leaked the payload they were given (issue #388).
 *
 *  The stopped event clears the connected nodes map, which would give memory back just before
 *  the event is made, so this runs with the map empty.
 */
TEST_CASE("The access point started and stopped events are not queued when they cannot be made", CLASS_OF_TESTS)
{
    WiFiRequestHandler *handler = TestWiFiEventState::Handler();
    MockMessageDispatcher *dispatcher = TestWiFiEventState::Dispatcher(handler);
    const bool madeEventGroup = TestWiFiEventState::GiveEventGroup();
    DrainOutbound(dispatcher);
    TestWiFiEventState::ClearConnectedNodes(handler);

    void *heap = ExhaustHeap();
    TestWiFiEventState::AccessPointStopped(handler);
    TestWiFiEventState::AccessPointStarted(handler);
    ReleaseHeap(heap);

    const int queued = dispatcher->OutboundMessageQueueLength();
    DrainOutbound(dispatcher);
    if (madeEventGroup)
    {
        TestWiFiEventState::ReleaseEventGroup();
    }
    TEST_ASSERT_EQUAL_MESSAGE(0, queued, "The started or stopped event was queued though it could not be made.");
}

/**
 * @brief Check the maximum connections event is dropped, not queued as a null message, when it cannot be made (issue #388).
 *
 *  The map already holds the station and a full access point, so adding the station needs no
 *  allocation: only the events are short of memory.  The connection event cannot make its
 *  payload, and the maximum connections event cannot be made.
 */
TEST_CASE("The maximum connections event is not queued when it cannot be made", CLASS_OF_TESTS)
{
    WiFiRequestHandler *handler = TestWiFiEventState::Handler();
    MockMessageDispatcher *dispatcher = TestWiFiEventState::Dispatcher(handler);
    DrainOutbound(dispatcher);
    TestWiFiEventState::FillConnectedNodes(handler, MAXIMUM_STATIONS - 1, TEST_STATION);

    void *heap = ExhaustHeap();
    TestWiFiEventState::NodeConnected(handler, TEST_STATION);
    ReleaseHeap(heap);

    const int queued = dispatcher->OutboundMessageQueueLength();
    DrainOutbound(dispatcher);
    TestWiFiEventState::ClearConnectedNodes(handler);
    TEST_ASSERT_EQUAL_MESSAGE(0, queued, "The maximum connections event was queued though it could not be made.");
}
