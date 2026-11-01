/**
 * @file Metrics.hpp
 * @author Mark Stevens
 * @brief Counters, gauges and latency histograms kept in every build.
 * @date 2026-10-31
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 *
 * @note The metrics are cheap enough to keep in a release build: recording one is a few atomic
 *       operations on statically allocated memory, with no allocation, no lock and no
 *       formatting.  They are read and reported elsewhere (the diagnostic snapshot).
 */

#pragma once

#include "sdkconfig.h"

#include <stddef.h>
#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"

/**
 * @brief Things that are counted.
 */
enum class MetricCounter : uint8_t
{
    /**
     * @brief SPI transactions with the STM32 that finished.
     */
    SpiTransactions = 0,

    /**
     * @brief Whole requests received from the STM32.
     */
    RequestsReceived,

    /**
     * @brief Responses put on the queue for the STM32.
     */
    ResponsesQueued,

    /**
     * @brief Events put on the queue for the STM32.
     */
    EventsQueued,

    /**
     * @brief Messages for the STM32 dropped because the queue stayed full.
     */
    MessagesDropped,

    /**
     * @brief Heap allocations that failed.
     */
    AllocationFailures,

    /**
     * @brief Number of counters; not a counter.
     */
    NumberOfCounters
};

/**
 * @brief Values that go up and down; the highest value is kept as well.
 */
enum class MetricGauge : uint8_t
{
    /**
     * @brief Messages waiting on the queue for the STM32.
     */
    OutboundQueueDepth = 0,

    /**
     * @brief Size of the most recent heap allocation that failed, in bytes.
     */
    FailedAllocationSize,

    /**
     * @brief Number of gauges; not a gauge.
     */
    NumberOfGauges
};

/**
 * @brief Times that are recorded as a distribution, in microseconds.
 */
enum class MetricHistogram : uint8_t
{
    /**
     * @brief WiFi request: arrival to the STM32 acknowledging the response.
     */
    WiFiRequestLatency = 0,

    /**
     * @brief Bluetooth request: arrival to the STM32 acknowledging the response.
     */
    BluetoothRequestLatency,

    /**
     * @brief System request: arrival to the STM32 acknowledging the response.
     */
    SystemRequestLatency,

    /**
     * @brief Time the WiFi request handler spends on a request.
     */
    WiFiHandlerTime,

    /**
     * @brief Time the Bluetooth request handler spends on a request.
     */
    BluetoothHandlerTime,

    /**
     * @brief Time the system request handler spends on a request.
     */
    SystemHandlerTime,

    /**
     * @brief Number of histograms; not a histogram.
     */
    NumberOfHistograms
};

/**
 * @brief What a histogram held when it was read.
 */
struct HistogramReading
{
    /**
     * @brief Number of buckets in a histogram.
     *
     * Bucket 0 holds times of 0 us and bucket n (n > 0) times from 2^(n - 1) us up to, but not
     * including, 2^n us, except the last, which holds everything from 2^(n - 1) us up.  With 24
     * buckets the last starts at about 4.2 seconds.
     */
    static constexpr size_t NUMBER_OF_BUCKETS = 24;

    /**
     * @brief Number of times recorded in each bucket.
     */
    uint32_t buckets[NUMBER_OF_BUCKETS];

    /**
     * @brief Number of times recorded.
     */
    uint32_t count;

    /**
     * @brief Sum of the times recorded, in microseconds.
     */
    uint64_t total;

    /**
     * @brief Longest time recorded, in microseconds.
     */
    uint32_t maximum;
};

/**
 * @brief The metrics kept by the firmware.
 */
class Metrics
{
public:
    /**
     * @brief Number of histogram buckets.
     */
    static constexpr size_t NUMBER_OF_BUCKETS = HistogramReading::NUMBER_OF_BUCKETS;

#if defined(CONFIG_MEADOW_METRICS)

    /**
     * @brief Start counting failed heap allocations.  Call once, early.
     *
     * The heap allows one failed allocation callback, so one that is wanted as well is passed
     * here and called after the failure is counted.
     *
     * @param alsoCall Callback to call for each failed allocation, or nullptr.
     */
    static void Setup(const esp_alloc_failed_hook_t alsoCall = nullptr);

    /**
     * @brief Add one to a counter.
     *
     * @param counter Counter to add to.
     */
    static void Increment(const MetricCounter counter);

    /**
     * @brief Set a gauge, raising its highest value if this is higher.
     *
     * @param gauge Gauge to set.
     * @param value New value.
     */
    static void Set(const MetricGauge gauge, const uint32_t value);

    /**
     * @brief Record a time in a histogram.
     *
     * @param histogram Histogram to record in.
     * @param microseconds Time to record.
     */
    static void Record(const MetricHistogram histogram, const uint32_t microseconds);

    /**
     * @brief Value of a counter.
     *
     * @param counter Counter to read.
     * @return The count.
     */
    static uint32_t Read(const MetricCounter counter);

    /**
     * @brief Value of a gauge.
     *
     * @param gauge Gauge to read.
     * @param maximum Set to the highest value the gauge has had.
     * @return The value now.
     */
    static uint32_t Read(const MetricGauge gauge, uint32_t &maximum);

    /**
     * @brief Read a histogram.
     *
     * The fields are read one at a time while other tasks may be recording, so a reading taken
     * during a burst can be off by the few times recorded while it was taken.
     *
     * @param histogram Histogram to read.
     * @param reading Filled in with what the histogram holds.
     */
    static void Read(const MetricHistogram histogram, HistogramReading &reading);

    /**
     * @brief Set every metric back to zero.
     */
    static void Reset();

#else

    /**
     * @brief Register the failed allocation callback given; nothing is counted.
     *
     * @param alsoCall Callback to call for each failed allocation, or nullptr.
     */
    static inline void Setup(const esp_alloc_failed_hook_t alsoCall = nullptr)
    {
        if (alsoCall != nullptr)
        {
            heap_caps_register_failed_alloc_callback(alsoCall);
        }
    }

    /**
     * @brief Add one to a counter (compiled out).
     */
    static inline void Increment(const MetricCounter)
    {
    }

    /**
     * @brief Set a gauge (compiled out).
     */
    static inline void Set(const MetricGauge, const uint32_t)
    {
    }

    /**
     * @brief Record a time in a histogram (compiled out).
     */
    static inline void Record(const MetricHistogram, const uint32_t)
    {
    }

#endif

    /**
     * @brief Bucket a time falls in.
     *
     * @param microseconds Time.
     * @return Index of the bucket, 0 to NUMBER_OF_BUCKETS - 1.
     */
    static size_t BucketFor(const uint32_t microseconds);

    /**
     * @brief Shortest time that falls in a bucket.
     *
     * @param bucket Index of the bucket.
     * @return Lower bound of the bucket in microseconds.
     */
    static uint32_t BucketLowerBound(const size_t bucket);

    /**
     * @brief Name of a counter, for reports.
     */
    static const char *Name(const MetricCounter counter);

    /**
     * @brief Name of a gauge, for reports.
     */
    static const char *Name(const MetricGauge gauge);

    /**
     * @brief Name of a histogram, for reports.
     */
    static const char *Name(const MetricHistogram histogram);

    /**
     * @brief Request latency histogram for an interface.
     *
     * @param interface Esp32Interfaces value.
     * @param histogram Set to the histogram when there is one.
     * @return true if the interface has a request latency histogram.
     */
    static bool RequestLatencyFor(const uint8_t interface, MetricHistogram &histogram);

    /**
     * @brief Handler time histogram for an interface.
     *
     * @param interface Esp32Interfaces value.
     * @param histogram Set to the histogram when there is one.
     * @return true if the interface has a handler time histogram.
     */
    static bool HandlerTimeFor(const uint8_t interface, MetricHistogram &histogram);
};

/**
 * @brief Records the time a scope takes in a histogram.
 *
 * The time runs from making the object to the end of the scope, so an early return is timed
 * like any other.  A timer made without a histogram records nothing.
 */
class MetricTimer
{
public:
    /**
     * @brief Start timing.
     *
     * @param histogram Histogram the time is recorded in.
     */
    explicit MetricTimer(const MetricHistogram histogram) : _histogram(histogram), _recording(true), _start(esp_timer_get_time())
    {
    }

    /**
     * @brief Start timing for a histogram that may not exist.
     *
     * @param histogram Histogram the time is recorded in.
     * @param recording false to record nothing.
     */
    MetricTimer(const MetricHistogram histogram, const bool recording) : _histogram(histogram), _recording(recording), _start(esp_timer_get_time())
    {
    }

    /**
     * @brief Record the time since the timer was made.
     */
    ~MetricTimer()
    {
        if (_recording)
        {
            Metrics::Record(_histogram, static_cast<uint32_t>(esp_timer_get_time() - _start));
        }
    }

    /**
     * @brief A timer cannot be copied: the copy would record the time a second time.
     */
    MetricTimer(const MetricTimer &) = delete;

    /**
     * @brief A timer cannot be assigned: the assignment would record the time a second time.
     */
    MetricTimer &operator=(const MetricTimer &) = delete;

private:
    /**
     * @brief Histogram the time is recorded in.
     */
    const MetricHistogram _histogram;

    /**
     * @brief false if the timer records nothing.
     */
    const bool _recording;

    /**
     * @brief Time the timer was made, from esp_timer_get_time.
     */
    const int64_t _start;
};
