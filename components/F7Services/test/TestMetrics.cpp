/*
 *  TestMetrics.cpp
 *
 *  Tests the counters, gauges and histograms in Metrics: the histogram buckets, recording and
 *  reading, recording from both cores at once, timing a scope, and counting failed allocations.
 */

#include "sdkconfig.h"

#include <stdint.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "unity.h"

#include "Metrics.hpp"
#include "SharedEnums.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Metrics][Emulated]"

/**
 * @brief Number of times each task records in the concurrency test.
 */
#define RECORDS_PER_TASK 20000

/**
 * @brief Time each task records in the concurrency test, in microseconds (bucket 5).
 */
#define RECORDED_TIME_US 20

/**
 * @brief Time the timed scope in the timer test takes, in microseconds.
 */
#define TIMED_SCOPE_US 20000

TEST_CASE("Each histogram bucket holds the times from its lower bound to the next", CLASS_OF_TESTS)
{
    TEST_ASSERT_EQUAL_UINT32(0, Metrics::BucketFor(0));
    TEST_ASSERT_EQUAL_UINT32(1, Metrics::BucketFor(1));
    TEST_ASSERT_EQUAL_UINT32(2, Metrics::BucketFor(2));
    TEST_ASSERT_EQUAL_UINT32(2, Metrics::BucketFor(3));
    TEST_ASSERT_EQUAL_UINT32(3, Metrics::BucketFor(4));
    for (size_t bucket = 1; bucket < (Metrics::NUMBER_OF_BUCKETS - 1); bucket++)
    {
        const uint32_t lower = Metrics::BucketLowerBound(bucket);
        const uint32_t next = Metrics::BucketLowerBound(bucket + 1);
        TEST_ASSERT_EQUAL_UINT32(bucket, Metrics::BucketFor(lower));
        TEST_ASSERT_EQUAL_UINT32(bucket, Metrics::BucketFor(next - 1));
    }
}

TEST_CASE("The last histogram bucket holds every time from its lower bound up", CLASS_OF_TESTS)
{
    const size_t last = Metrics::NUMBER_OF_BUCKETS - 1;
    TEST_ASSERT_EQUAL_UINT32(last, Metrics::BucketFor(Metrics::BucketLowerBound(last)));
    TEST_ASSERT_EQUAL_UINT32(last, Metrics::BucketFor(UINT32_MAX));
}

TEST_CASE("Every interface with a request handler has its histograms", CLASS_OF_TESTS)
{
    MetricHistogram histogram;
    TEST_ASSERT_TRUE(Metrics::RequestLatencyFor(Esp32Interfaces::WiFi, histogram));
    TEST_ASSERT_EQUAL(MetricHistogram::WiFiRequestLatency, histogram);
    TEST_ASSERT_TRUE(Metrics::RequestLatencyFor(Esp32Interfaces::BlueTooth, histogram));
    TEST_ASSERT_EQUAL(MetricHistogram::BluetoothRequestLatency, histogram);
    TEST_ASSERT_TRUE(Metrics::HandlerTimeFor(Esp32Interfaces::System, histogram));
    TEST_ASSERT_EQUAL(MetricHistogram::SystemHandlerTime, histogram);
    TEST_ASSERT_FALSE(Metrics::RequestLatencyFor(Esp32Interfaces::Transport, histogram));
    TEST_ASSERT_FALSE(Metrics::HandlerTimeFor(Esp32Interfaces::None, histogram));
}

#if defined(CONFIG_MEADOW_METRICS)

TEST_CASE("Counters, gauges and histograms record what they are given and reset to zero", CLASS_OF_TESTS)
{
    Metrics::Reset();
    Metrics::Increment(MetricCounter::RequestsReceived);
    Metrics::Increment(MetricCounter::RequestsReceived);
    TEST_ASSERT_EQUAL_UINT32(2, Metrics::Read(MetricCounter::RequestsReceived));

    uint32_t maximum = 0;
    Metrics::Set(MetricGauge::OutboundQueueDepth, 7);
    Metrics::Set(MetricGauge::OutboundQueueDepth, 3);
    TEST_ASSERT_EQUAL_UINT32(3, Metrics::Read(MetricGauge::OutboundQueueDepth, maximum));
    TEST_ASSERT_EQUAL_UINT32(7, maximum);

    Metrics::Record(MetricHistogram::WiFiHandlerTime, 0);
    Metrics::Record(MetricHistogram::WiFiHandlerTime, 1500);
    Metrics::Record(MetricHistogram::WiFiHandlerTime, 1500);
    HistogramReading reading;
    Metrics::Read(MetricHistogram::WiFiHandlerTime, reading);
    TEST_ASSERT_EQUAL_UINT32(3, reading.count);
    TEST_ASSERT_TRUE_MESSAGE(reading.total == 3000, "The total is wrong.");
    TEST_ASSERT_EQUAL_UINT32(1500, reading.maximum);
    TEST_ASSERT_EQUAL_UINT32(1, reading.buckets[0]);
    TEST_ASSERT_EQUAL_UINT32(2, reading.buckets[Metrics::BucketFor(1500)]);

    Metrics::Reset();
    TEST_ASSERT_EQUAL_UINT32(0, Metrics::Read(MetricCounter::RequestsReceived));
    TEST_ASSERT_EQUAL_UINT32(0, Metrics::Read(MetricGauge::OutboundQueueDepth, maximum));
    TEST_ASSERT_EQUAL_UINT32(0, maximum);
    Metrics::Read(MetricHistogram::WiFiHandlerTime, reading);
    TEST_ASSERT_EQUAL_UINT32(0, reading.count);
    TEST_ASSERT_TRUE_MESSAGE(reading.total == 0, "The total was not reset.");
    TEST_ASSERT_EQUAL_UINT32(0, reading.buckets[0]);
}

/**
 * @brief Semaphore the recording tasks wait on, so that they start together.
 */
static SemaphoreHandle_t _startGate = nullptr;

/**
 * @brief Semaphore each recording task gives when it has finished.
 */
static SemaphoreHandle_t _finished = nullptr;

/**
 * @brief Records RECORDS_PER_TASK times and counts as fast as it can.
 *
 * @param parameters Not used.
 */
static void RecordingTask(void *parameters)
{
    xSemaphoreTake(_startGate, portMAX_DELAY);
    for (uint32_t index = 0; index < RECORDS_PER_TASK; index++)
    {
        Metrics::Record(MetricHistogram::SystemHandlerTime, RECORDED_TIME_US);
        Metrics::Increment(MetricCounter::SpiTransactions);
    }
    xSemaphoreGive(_finished);
    vTaskSuspend(nullptr);
}

TEST_CASE("Metrics recorded from both cores at once are all counted", CLASS_OF_TESTS)
{
    Metrics::Reset();
    _startGate = xSemaphoreCreateCounting(2, 0);
    _finished = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(_startGate);
    TEST_ASSERT_NOT_NULL(_finished);

    //
    //  The tasks run below this task's priority and wait at the gate, so neither starts until
    //  both are made and the gate is opened for both at once.
    //
    TaskHandle_t tasks[2] = {nullptr, nullptr};
    const UBaseType_t priority = uxTaskPriorityGet(nullptr) - 1;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(RecordingTask, "Record0", 2048, nullptr, priority, &tasks[0], 0));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(RecordingTask, "Record1", 2048, nullptr, priority, &tasks[1], 1));
    xSemaphoreGive(_startGate);
    xSemaphoreGive(_startGate);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(_finished, pdMS_TO_TICKS(30000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(_finished, pdMS_TO_TICKS(30000)));
    vTaskDelete(tasks[0]);
    vTaskDelete(tasks[1]);
    vSemaphoreDelete(_startGate);
    vSemaphoreDelete(_finished);
    _startGate = nullptr;
    _finished = nullptr;

    HistogramReading reading;
    Metrics::Read(MetricHistogram::SystemHandlerTime, reading);
    TEST_ASSERT_EQUAL_UINT32(2 * RECORDS_PER_TASK, reading.count);
    TEST_ASSERT_TRUE_MESSAGE(reading.total == (2ULL * RECORDS_PER_TASK * RECORDED_TIME_US), "The total is wrong.");
    TEST_ASSERT_EQUAL_UINT32(2 * RECORDS_PER_TASK, reading.buckets[Metrics::BucketFor(RECORDED_TIME_US)]);
    TEST_ASSERT_EQUAL_UINT32(2 * RECORDS_PER_TASK, Metrics::Read(MetricCounter::SpiTransactions));
    Metrics::Reset();
}

TEST_CASE("A metric timer records the time its scope took", CLASS_OF_TESTS)
{
    Metrics::Reset();
    //
    //  The scope spins rather than delays: a delay of n ticks can return almost a whole tick
    //  early, which made the lower bound unreliable under QEMU.
    //
    {
        MetricTimer timer(MetricHistogram::BluetoothHandlerTime);
        const int64_t end = esp_timer_get_time() + TIMED_SCOPE_US;
        while (esp_timer_get_time() < end)
        {
        }
    }
    {
        MetricTimer timer(MetricHistogram::BluetoothHandlerTime, false);
    }
    HistogramReading reading;
    Metrics::Read(MetricHistogram::BluetoothHandlerTime, reading);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, reading.count, "The timer that was told not to record did.");
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(TIMED_SCOPE_US, reading.maximum);
    TEST_ASSERT_LESS_THAN_UINT32(10 * TIMED_SCOPE_US, reading.maximum);
    Metrics::Reset();
}

/**
 * @brief Number of times the callback passed to Metrics::Setup has been called.
 */
static uint32_t _alsoCalled = 0;

/**
 * @brief Failed allocation callback that counts the calls.
 */
static void CountFailedAllocation(size_t size, uint32_t caps, const char *functionName)
{
    _alsoCalled++;
}

TEST_CASE("A failed allocation is counted and the other callback is still called", CLASS_OF_TESTS)
{
    Metrics::Reset();
    _alsoCalled = 0;
    Metrics::Setup(CountFailedAllocation);

    const size_t impossible = 8 * 1024 * 1024;
    void *memory = heap_caps_malloc(impossible, MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NULL_MESSAGE(memory, "An 8 MB allocation succeeded.");

    uint32_t maximum = 0;
    TEST_ASSERT_EQUAL_UINT32(1, Metrics::Read(MetricCounter::AllocationFailures));
    TEST_ASSERT_EQUAL_UINT32(impossible, Metrics::Read(MetricGauge::FailedAllocationSize, maximum));
    TEST_ASSERT_EQUAL_UINT32(1, _alsoCalled);

    Metrics::Setup();
    Metrics::Reset();
}

#endif // CONFIG_MEADOW_METRICS
