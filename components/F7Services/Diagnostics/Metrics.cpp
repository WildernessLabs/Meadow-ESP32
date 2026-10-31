/**
 * @file Metrics.cpp
 * @author Mark Stevens
 * @brief Counters, gauges and latency histograms kept in every build.
 * @date 2026-10-31
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <atomic>

#include "esp_heap_caps.h"

#include "Metrics.hpp"
#include "SharedEnums.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                     Initialise static data.
 *
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Names of the counters, in the order of MetricCounter.
 */
static const char *COUNTER_NAMES[] = {"SpiTransactions", "RequestsReceived", "ResponsesQueued", "EventsQueued", "MessagesDropped", "AllocationFailures"};
static_assert(sizeof(COUNTER_NAMES) / sizeof(COUNTER_NAMES[0]) == static_cast<size_t>(MetricCounter::NumberOfCounters), "A counter has no name.");

/**
 * @brief Names of the gauges, in the order of MetricGauge.
 */
static const char *GAUGE_NAMES[] = {"OutboundQueueDepth", "FailedAllocationSize"};
static_assert(sizeof(GAUGE_NAMES) / sizeof(GAUGE_NAMES[0]) == static_cast<size_t>(MetricGauge::NumberOfGauges), "A gauge has no name.");

/**
 * @brief Names of the histograms, in the order of MetricHistogram.
 */
static const char *HISTOGRAM_NAMES[] = {"WiFiRequestLatency", "BluetoothRequestLatency", "SystemRequestLatency", "WiFiHandlerTime", "BluetoothHandlerTime", "SystemHandlerTime"};
static_assert(sizeof(HISTOGRAM_NAMES) / sizeof(HISTOGRAM_NAMES[0]) == static_cast<size_t>(MetricHistogram::NumberOfHistograms), "A histogram has no name.");

#if defined(CONFIG_MEADOW_METRICS)

/**
 * @brief A gauge: the value now and the highest it has been.
 */
struct GaugeData
{
    /**
     * @brief Value now.
     */
    std::atomic<uint32_t> value;

    /**
     * @brief Highest value.
     */
    std::atomic<uint32_t> maximum;
};

/**
 * @brief A histogram of times.
 */
struct HistogramData
{
    /**
     * @brief Number of times in each bucket.
     */
    std::atomic<uint32_t> buckets[Metrics::NUMBER_OF_BUCKETS];

    /**
     * @brief Number of times recorded.
     */
    std::atomic<uint32_t> count;

    /**
     * @brief Sum of the times recorded, in microseconds.
     */
    std::atomic<uint64_t> total;

    /**
     * @brief Longest time recorded, in microseconds.
     */
    std::atomic<uint32_t> maximum;
};

/**
 * @brief The counters.
 */
static std::atomic<uint32_t> _counters[static_cast<size_t>(MetricCounter::NumberOfCounters)];

/**
 * @brief The gauges.
 */
static GaugeData _gauges[static_cast<size_t>(MetricGauge::NumberOfGauges)];

/**
 * @brief The histograms.
 */
static HistogramData _histograms[static_cast<size_t>(MetricHistogram::NumberOfHistograms)];

/**
 * @brief Failed allocation callback to call after counting the failure, or nullptr.
 */
static esp_alloc_failed_hook_t _alsoCall = nullptr;

/*
 * ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Raise a maximum to a value if the value is higher.
 *
 * @param maximum Maximum to raise.
 * @param value Value to compare with it.
 */
static void RaiseMaximum(std::atomic<uint32_t> &maximum, const uint32_t value)
{
    uint32_t current = maximum.load(std::memory_order_relaxed);
    while ((value > current) && !maximum.compare_exchange_weak(current, value, std::memory_order_relaxed))
    {
    }
}

/**
 * @brief Count a failed heap allocation.
 *
 * Called by the heap allocator when an allocation fails, before it returns nullptr (or, in a
 * build with CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS, aborts).  It must not allocate.
 *
 * @param size Number of bytes asked for.
 * @param caps Capabilities asked for.
 * @param functionName Name of the allocation function that failed.
 */
static void AllocationFailed(size_t size, uint32_t caps, const char *functionName)
{
    Metrics::Increment(MetricCounter::AllocationFailures);
    Metrics::Set(MetricGauge::FailedAllocationSize, static_cast<uint32_t>(size));
    if (_alsoCall != nullptr)
    {
        _alsoCall(size, caps, functionName);
    }
}

/**
 * @brief Start counting failed heap allocations.  Call once, early.
 *
 * @param alsoCall Callback to call for each failed allocation, or nullptr.
 */
void Metrics::Setup(const esp_alloc_failed_hook_t alsoCall)
{
    _alsoCall = alsoCall;
    heap_caps_register_failed_alloc_callback(AllocationFailed);
}

/**
 * @brief Add one to a counter.
 *
 * @param counter Counter to add to.
 */
void Metrics::Increment(const MetricCounter counter)
{
    _counters[static_cast<size_t>(counter)].fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief Set a gauge, raising its highest value if this is higher.
 *
 * @param gauge Gauge to set.
 * @param value New value.
 */
void Metrics::Set(const MetricGauge gauge, const uint32_t value)
{
    GaugeData &data = _gauges[static_cast<size_t>(gauge)];
    data.value.store(value, std::memory_order_relaxed);
    RaiseMaximum(data.maximum, value);
}

/**
 * @brief Record a time in a histogram.
 *
 * @param histogram Histogram to record in.
 * @param microseconds Time to record.
 */
void Metrics::Record(const MetricHistogram histogram, const uint32_t microseconds)
{
    HistogramData &data = _histograms[static_cast<size_t>(histogram)];
    data.buckets[BucketFor(microseconds)].fetch_add(1, std::memory_order_relaxed);
    data.count.fetch_add(1, std::memory_order_relaxed);
    data.total.fetch_add(microseconds, std::memory_order_relaxed);
    RaiseMaximum(data.maximum, microseconds);
}

/**
 * @brief Value of a counter.
 *
 * @param counter Counter to read.
 * @return The count.
 */
uint32_t Metrics::Read(const MetricCounter counter)
{
    return (_counters[static_cast<size_t>(counter)].load(std::memory_order_relaxed));
}

/**
 * @brief Value of a gauge.
 *
 * @param gauge Gauge to read.
 * @param maximum Set to the highest value the gauge has had.
 * @return The value now.
 */
uint32_t Metrics::Read(const MetricGauge gauge, uint32_t &maximum)
{
    const GaugeData &data = _gauges[static_cast<size_t>(gauge)];
    maximum = data.maximum.load(std::memory_order_relaxed);
    return (data.value.load(std::memory_order_relaxed));
}

/**
 * @brief Read a histogram.
 *
 * @param histogram Histogram to read.
 * @param reading Filled in with what the histogram holds.
 */
void Metrics::Read(const MetricHistogram histogram, HistogramReading &reading)
{
    const HistogramData &data = _histograms[static_cast<size_t>(histogram)];
    for (size_t bucket = 0; bucket < NUMBER_OF_BUCKETS; bucket++)
    {
        reading.buckets[bucket] = data.buckets[bucket].load(std::memory_order_relaxed);
    }
    reading.count = data.count.load(std::memory_order_relaxed);
    reading.total = data.total.load(std::memory_order_relaxed);
    reading.maximum = data.maximum.load(std::memory_order_relaxed);
}

/**
 * @brief Set every metric back to zero.
 *
 * A time being recorded while the reset runs may survive it in part.
 */
void Metrics::Reset()
{
    for (std::atomic<uint32_t> &counter : _counters)
    {
        counter.store(0, std::memory_order_relaxed);
    }
    for (GaugeData &gauge : _gauges)
    {
        gauge.value.store(0, std::memory_order_relaxed);
        gauge.maximum.store(0, std::memory_order_relaxed);
    }
    for (HistogramData &histogram : _histograms)
    {
        for (std::atomic<uint32_t> &bucket : histogram.buckets)
        {
            bucket.store(0, std::memory_order_relaxed);
        }
        histogram.count.store(0, std::memory_order_relaxed);
        histogram.total.store(0, std::memory_order_relaxed);
        histogram.maximum.store(0, std::memory_order_relaxed);
    }
}

#endif // CONFIG_MEADOW_METRICS

/**
 * @brief Bucket a time falls in.
 *
 * @param microseconds Time.
 * @return Index of the bucket, 0 to NUMBER_OF_BUCKETS - 1.
 */
size_t Metrics::BucketFor(const uint32_t microseconds)
{
    if (microseconds == 0)
    {
        return (0);
    }
    const size_t bucket = static_cast<size_t>(32 - __builtin_clz(microseconds));
    return ((bucket < NUMBER_OF_BUCKETS) ? bucket : (NUMBER_OF_BUCKETS - 1));
}

/**
 * @brief Shortest time that falls in a bucket.
 *
 * @param bucket Index of the bucket.
 * @return Lower bound of the bucket in microseconds.
 */
uint32_t Metrics::BucketLowerBound(const size_t bucket)
{
    return ((bucket == 0) ? 0 : (static_cast<uint32_t>(1) << (bucket - 1)));
}

/**
 * @brief Name of a counter, for reports.
 *
 * @param counter Counter.
 * @return Its name.
 */
const char *Metrics::Name(const MetricCounter counter)
{
    return (COUNTER_NAMES[static_cast<size_t>(counter)]);
}

/**
 * @brief Name of a gauge, for reports.
 *
 * @param gauge Gauge.
 * @return Its name.
 */
const char *Metrics::Name(const MetricGauge gauge)
{
    return (GAUGE_NAMES[static_cast<size_t>(gauge)]);
}

/**
 * @brief Name of a histogram, for reports.
 *
 * @param histogram Histogram.
 * @return Its name.
 */
const char *Metrics::Name(const MetricHistogram histogram)
{
    return (HISTOGRAM_NAMES[static_cast<size_t>(histogram)]);
}

/**
 * @brief Request latency histogram for an interface.
 *
 * @param interface Esp32Interfaces value.
 * @param histogram Set to the histogram when there is one.
 * @return true if the interface has a request latency histogram.
 */
bool Metrics::RequestLatencyFor(const uint8_t interface, MetricHistogram &histogram)
{
    switch (interface)
    {
        case Esp32Interfaces::WiFi:
            histogram = MetricHistogram::WiFiRequestLatency;
            return (true);
        case Esp32Interfaces::BlueTooth:
            histogram = MetricHistogram::BluetoothRequestLatency;
            return (true);
        case Esp32Interfaces::System:
            histogram = MetricHistogram::SystemRequestLatency;
            return (true);
        default:
            return (false);
    }
}

/**
 * @brief Handler time histogram for an interface.
 *
 * @param interface Esp32Interfaces value.
 * @param histogram Set to the histogram when there is one.
 * @return true if the interface has a handler time histogram.
 */
bool Metrics::HandlerTimeFor(const uint8_t interface, MetricHistogram &histogram)
{
    switch (interface)
    {
        case Esp32Interfaces::WiFi:
            histogram = MetricHistogram::WiFiHandlerTime;
            return (true);
        case Esp32Interfaces::BlueTooth:
            histogram = MetricHistogram::BluetoothHandlerTime;
            return (true);
        case Esp32Interfaces::System:
            histogram = MetricHistogram::SystemHandlerTime;
            return (true);
        default:
            return (false);
    }
}
