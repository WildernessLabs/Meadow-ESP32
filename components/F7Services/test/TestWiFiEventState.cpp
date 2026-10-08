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
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi_default.h"
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
     * @brief The station network interface the handler holds.
     */
    static esp_netif_t *StationNetworkInterface()
    {
        return (WiFiRequestHandler::_stationNetIfHandle);
    }

    /**
     * @brief Give the handler a station network interface to hold.
     */
    static void SetStationNetworkInterface(esp_netif_t *networkInterface)
    {
        WiFiRequestHandler::_stationNetIfHandle = networkInterface;
    }

    /**
     * @brief Call the private DestroyNetworkInterfaces method.
     */
    static void DestroyNetworkInterfaces(WiFiRequestHandler *handler)
    {
        handler->DestroyNetworkInterfaces();
    }

    /**
     * @brief Call the private StationStoppedEvent method, with an event group for it to clear bits in.
     */
    static void StationStopped(WiFiRequestHandler *handler)
    {
        const bool madeEventGroup = (WiFiRequestHandler::_xWiFiEventGroup == nullptr);
        if (madeEventGroup)
        {
            WiFiRequestHandler::_xWiFiEventGroup = xEventGroupCreate();
            TEST_ASSERT_NOT_NULL(WiFiRequestHandler::_xWiFiEventGroup);
        }
        handler->StationStoppedEvent();
        if (madeEventGroup)
        {
            vEventGroupDelete(WiFiRequestHandler::_xWiFiEventGroup);
            WiFiRequestHandler::_xWiFiEventGroup = nullptr;
        }
    }

    /**
     * @brief Call the private StartWiFiInterface method, with an event group for it to use.
     *
     * @param handler Handler to start.
     * @param mode Mode to start it in.
     *
     * @return What StartWiFiInterface returned.
     */
    static esp_err_t StartInterface(WiFiRequestHandler *handler, const wifi_mode_t mode)
    {
        if (WiFiRequestHandler::_xWiFiEventGroup == nullptr)
        {
            WiFiRequestHandler::_xWiFiEventGroup = xEventGroupCreate();
            TEST_ASSERT_NOT_NULL(WiFiRequestHandler::_xWiFiEventGroup);
        }
        return (handler->StartWiFiInterface(mode, nullptr));
    }

    /**
     * @brief Call the private StopWiFIAdapter method.
     */
    static StatusCodes::StatusCodes StopAdapter(WiFiRequestHandler *handler)
    {
        return (handler->StopWiFIAdapter());
    }

    /**
     * @brief Whether the handler's event group holds the error bit and the starting bit.
     *
     * @param error Set to whether WIFI_ERROR_BIT is set.
     * @param starting Set to whether WIFI_INTERFACE_STARTING_BIT is set.
     * @param started Set to whether WIFI_INTERFACE_STARTED_BIT is set.
     */
    static void StartBits(bool &error, bool &starting, bool &started)
    {
        const EventBits_t bits = xEventGroupGetBits(WiFiRequestHandler::_xWiFiEventGroup);
        error = ((bits & WiFiRequestHandler::WIFI_ERROR_BIT) != 0);
        starting = ((bits & WiFiRequestHandler::WIFI_INTERFACE_STARTING_BIT) != 0);
        started = ((bits & WiFiRequestHandler::WIFI_INTERFACE_STARTED_BIT) != 0);
    }

    /**
     * @brief Clear the event group's bits and delete it, so no state is left for another test.
     */
    static void ReleaseStartState()
    {
        xEventGroupClearBits(WiFiRequestHandler::_xWiFiEventGroup, 0xffffff);
        vEventGroupDelete(WiFiRequestHandler::_xWiFiEventGroup);
        WiFiRequestHandler::_xWiFiEventGroup = nullptr;
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
     * @brief Call the private NodeDisconnectedEvent method for a station.
     *
     * @param handler Handler to call it on.
     * @param lastByte Last byte of the station's address, the only one that varies.
     */
    static void NodeDisconnected(WiFiRequestHandler *handler, const uint8_t lastByte)
    {
        wifi_event_ap_stadisconnected_t event = {};
        event.mac[0] = 0x02;
        event.mac[5] = lastByte;
        handler->NodeDisconnectedEvent(&event);
    }

    /**
     * @brief Whether the connected nodes list is locked by a task.
     *
     * @param handler Handler whose list is checked.
     */
    static bool ConnectedNodesLocked(WiFiRequestHandler *handler)
    {
        return (xSemaphoreGetMutexHolder(handler->_connectedNodesListMutex) != nullptr);
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
 * @brief Make the station network interface the way StartWiFiInterface does.
 *
 *  esp_netif_init() does nothing the second time, and the default event loop is made once
 *  and kept, which is what StartWiFiInterface now accepts.
 *
 * @return The network interface.
 */
static esp_netif_t *MakeStationNetworkInterface()
{
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_init());
    const esp_err_t loop = esp_event_loop_create_default();
    TEST_ASSERT_TRUE_MESSAGE((loop == ESP_OK) || (loop == ESP_ERR_INVALID_STATE), "The default event loop could not be made.");
    esp_netif_t *networkInterface = esp_netif_create_default_wifi_sta();
    TEST_ASSERT_NOT_NULL(networkInterface);
    return (networkInterface);
}

/**
 * @brief Check the network interfaces are destroyed, so another can be made (issue #292).
 *
 *  A station network interface is made every time the WiFi interface starts, and making a
 *  second while the first exists is refused with a duplicate key and aborts the board.  Before
 *  this the stop path only set the handle to nullptr.
 */
TEST_CASE("Destroying the network interfaces lets the station one be made again", CLASS_OF_TESTS)
{
    WiFiRequestHandler *handler = TestWiFiEventState::Handler();

    TestWiFiEventState::SetStationNetworkInterface(MakeStationNetworkInterface());
    TestWiFiEventState::DestroyNetworkInterfaces(handler);
    TEST_ASSERT_NULL_MESSAGE(TestWiFiEventState::StationNetworkInterface(), "The station network interface handle was not cleared.");

    //
    //  Making another with the same key aborts the board if the first still exists.
    //
    esp_netif_t *second = MakeStationNetworkInterface();
    TestWiFiEventState::SetStationNetworkInterface(second);
    TestWiFiEventState::DestroyNetworkInterfaces(handler);
    TEST_ASSERT_NULL(TestWiFiEventState::StationNetworkInterface());

    //
    //  There is nothing to destroy the third time, which must not be an error.
    //
    TestWiFiEventState::DestroyNetworkInterfaces(handler);
}

/**
 * @brief Check the station stopped event leaves the station network interface for the stop path to destroy (issue #292).
 *
 *  The event used to set the handle to nullptr, so by the time StopWiFIAdapter() came to
 *  destroy the interface there was no handle to destroy it with.
 */
TEST_CASE("The station stopped event keeps the station network interface", CLASS_OF_TESTS)
{
    WiFiRequestHandler *handler = TestWiFiEventState::Handler();

    esp_netif_t *networkInterface = MakeStationNetworkInterface();
    TestWiFiEventState::SetStationNetworkInterface(networkInterface);
    TestWiFiEventState::StationStopped(handler);
    esp_netif_t *held = TestWiFiEventState::StationNetworkInterface();

    TestWiFiEventState::DestroyNetworkInterfaces(handler);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(networkInterface, held, "The station stopped event dropped the station network interface.");
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

/**
 * @brief Check an interface that cannot start returns the error, and the next start works (issue #248).
 *
 *  StartWiFiInterface returned its error to callers that checked it, then called
 *  ESP_ERROR_CHECK on it, which reset the board.  A mode the driver refuses is a start that
 *  fails; the callers answer the STM32 with CannotStartNetworkInterface.
 *
 *  This starts the radio, so it is hardware only.
 */
TEST_CASE("An interface that cannot start returns the error and the next start works", "[WiFiEventState][HardwareOnly]")
{
    WiFiRequestHandler *handler = TestWiFiEventState::Handler();

    const esp_err_t failed = TestWiFiEventState::StartInterface(handler, WIFI_MODE_MAX);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(ESP_OK, failed, "The driver accepted a mode it should refuse.");
    bool error = false;
    bool starting = false;
    bool started = false;
    TestWiFiEventState::StartBits(error, starting, started);
    TEST_ASSERT_TRUE_MESSAGE(error, "The failed start did not set the error bit.");
    TEST_ASSERT_FALSE_MESSAGE(starting, "The failed start left the starting bit set.");
    TEST_ASSERT_FALSE_MESSAGE(started, "The failed start set the started bit.");

    //
    //  Nothing the failed start left behind stops the next one.
    //
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, TestWiFiEventState::StartInterface(handler, WIFI_MODE_STA), "The start after a failed start failed too.");
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, TestWiFiEventState::StopAdapter(handler));
    TestWiFiEventState::ReleaseStartState();
}

/**
 * @brief Set when the connected nodes list was locked as an event was queued for the STM32.
 */
static bool _lockedWhileQueuing = false;

/**
 * @brief Called as an event is queued: note whether the connected nodes list is locked.
 *
 * @param context The handler whose list is checked.
 */
static void NoteWhetherLocked(void *context)
{
    _lockedWhileQueuing |= TestWiFiEventState::ConnectedNodesLocked(static_cast<WiFiRequestHandler *>(context));
}

/**
 * @brief Check the connected nodes list is not locked while the node events are queued (issue #550).
 *
 *  Queuing an event allocates, takes the outbound queue's mutex, writes to the UART and can
 *  wait for room in the queue, so holding the list's mutex across it blocked the event task and
 *  everything else that wanted the list.
 */
TEST_CASE("The connected nodes list is not locked while the node events are queued", CLASS_OF_TESTS)
{
    WiFiRequestHandler *handler = TestWiFiEventState::Handler();
    MockMessageDispatcher *dispatcher = TestWiFiEventState::Dispatcher(handler);
    TestWiFiEventState::ClearConnectedNodes(handler);
    DrainOutbound(dispatcher);
    dispatcher->QueueObserver = NoteWhetherLocked;
    dispatcher->QueueObserverContext = handler;
    _lockedWhileQueuing = false;

    //
    //  The connect event, and the maximum connections event that follows it when the access
    //  point fills up.
    //
    TestWiFiEventState::FillConnectedNodes(handler, MAXIMUM_STATIONS - 1, -1);
    TestWiFiEventState::NodeConnected(handler, TEST_STATION);
    Message *event0 = NextEvent(dispatcher, WiFiFunction::NodeConnectedEvent);
    Message::DeleteMessage(event0);
    Message *event1 = NextEvent(dispatcher, WiFiFunction::MaximumConnectionsReachedEvent);
    Message::DeleteMessage(event1);
    TEST_ASSERT_FALSE_MESSAGE(_lockedWhileQueuing, "The connected nodes list was locked while a connect event was queued.");

    TestWiFiEventState::NodeDisconnected(handler, TEST_STATION);
    Message *event2 = NextEvent(dispatcher, WiFiFunction::NodeDisconnectedEvent);
    Message::DeleteMessage(event2);
    TEST_ASSERT_FALSE_MESSAGE(_lockedWhileQueuing, "The connected nodes list was locked while a disconnect event was queued.");

    dispatcher->QueueObserver = nullptr;
    dispatcher->QueueObserverContext = nullptr;
    TestWiFiEventState::ClearConnectedNodes(handler);
    DrainOutbound(dispatcher);
}
