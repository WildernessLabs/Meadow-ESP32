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
     * @brief Restore an empty access point record.
     *
     * @param handler Handler to clear.
     */
    static void Clear(WiFiRequestHandler *handler)
    {
        const wifi_event_sta_connected_t empty = {};
        StoreAccessPoint(handler, empty);
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
