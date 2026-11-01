/**
 * @file Diagnostics.cpp
 * @author Mark Stevens
 * @brief Provide system diagnostic tools for the ESP32 application.
 * @date 2025-06-04
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 *
 * @note The records are written with ESP_LOGI rather than the trace macros in Logging.hpp, so
 *       that they do not need the --tracemessages flag, and they go wherever the log goes
 *       (UART, UDP or SystemView).
 */

#include "sdkconfig.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#if defined(CONFIG_HEAP_TASK_TRACKING)
    #include "esp_heap_task_info.h"
#endif

#if defined(CONFIG_LWIP_STATS)
    #include "lwip/stats.h"
#endif

#include "Diagnostics.hpp"
#include "Metrics.hpp"
#include "Utils.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                     Initialise static data.
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component; the records are logged under it.
 */
const char *Diagnostics::COMPONENT_NAME = "Diagnostics";

/**
 *  @brief Run time of each task at the last report.
 */
Diagnostics::RunTimeSample Diagnostics::_previousRunTimes[Diagnostics::MAXIMUM_TRACKED_TASKS];

/**
 *  @brief Number of entries in _previousRunTimes that are in use.
 */
size_t Diagnostics::_numberOfPreviousRunTimes = 0;

/**
 *  @brief Total run time (time since boot) at the last report.
 */
configRUN_TIME_COUNTER_TYPE Diagnostics::_previousTotalRunTime = 0;

/**
 *  @brief Sequence number of the last snapshot taken, 0 before the first.
 */
uint32_t Diagnostics::_sequence = 0;

/**
 *  @brief The snapshot task, nullptr until Setup has made it.
 */
TaskHandle_t Diagnostics::_taskHandle = nullptr;

/**
 *  @brief Period between snapshots in milliseconds, 0 for snapshots only when asked for.
 */
uint32_t Diagnostics::_period = 0;

/*
 * ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Start the snapshot task.  Calling it again does nothing.
 *
 * @param periodMilliseconds Time between snapshots, or 0 to take them only when asked for.
 */
void Diagnostics::Setup(const uint32_t periodMilliseconds)
{
    if (_taskHandle != nullptr)
    {
        return;
    }
    _period = ((periodMilliseconds > 0) && (periodMilliseconds < MINIMUM_PERIOD_MS)) ? MINIMUM_PERIOD_MS : periodMilliseconds;
    if (xTaskCreate(Task, "Diagnostics", TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &_taskHandle) != pdPASS)
    {
        _taskHandle = nullptr;
        ESP_LOGE(COMPONENT_NAME, "Failed to create the diagnostics task");
    }
}

/**
 * @brief Ask the snapshot task to take a snapshot now.  Does nothing before Setup.
 */
void Diagnostics::RequestSnapshot()
{
    if (_taskHandle != nullptr)
    {
        xTaskNotifyGive(_taskHandle);
    }
}

/**
 * @brief Sequence number of the last snapshot taken, 0 before the first.
 *
 * @return The sequence number.
 */
uint32_t Diagnostics::LastSequence()
{
    return (_sequence);
}

/**
 * @brief The snapshot task: takes a snapshot each period, or when asked.
 *
 * It waits for a notification, for at most the period, so a request is answered at once and
 * a periodic snapshot is taken when the wait runs out.  The period is counted from the end of
 * one snapshot to the start of the next.
 *
 * @param parameters Not used.
 */
void Diagnostics::Task(void *parameters)
{
    const TickType_t wait = (_period == 0) ? portMAX_DELAY : pdMS_TO_TICKS(_period);
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, wait);
        TakeSnapshot();
    }
}

/**
 * @brief Mutex that stops two snapshots being written at once.
 *
 * A function-local static is created exactly once, on first use, even when several tasks get
 * here at the same time.
 *
 * @return The mutex.
 */
SemaphoreHandle_t Diagnostics::SnapshotMutex()
{
    static const SemaphoreHandle_t mutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the diagnostics snapshot mutex");
    return (mutex);
}

/**
 * @brief Take a snapshot now, in the calling task, and write its records.
 *
 * The records are, in order: snapshot (the header), task (one per task), cpu, heap (one per
 * kind of memory), taskheap, counter, gauge, histogram, network, and end, which gives the
 * time the snapshot took.  Records for features not built in are left out.
 *
 * @return Sequence number of the snapshot.
 */
uint32_t Diagnostics::TakeSnapshot()
{
    const SemaphoreHandle_t mutex = SnapshotMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);

    esp_log_level_set(COMPONENT_NAME, ESP_LOG_INFO);
    const int64_t start = esp_timer_get_time();
    const uint32_t sequence = ++_sequence;
    WriteRecord("snapshot,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32, sequence, static_cast<uint32_t>(start / 1000), static_cast<uint32_t>(esp_reset_reason()), static_cast<uint32_t>(esp_get_free_heap_size()));
    WriteTasks(sequence);
    WriteHeap(sequence);
    WriteTaskHeap(sequence);
    WriteMetrics(sequence);
    WriteNetwork(sequence);
    WriteRecord("end,%" PRIu32 ",%" PRIu32, sequence, static_cast<uint32_t>(esp_timer_get_time() - start));

    xSemaphoreGive(mutex);
    return (sequence);
}

/**
 * @brief Write one record: @diag, then the values formatted as for printf.
 *
 * The record is formatted on the stack, so writing one does not allocate.
 *
 * @param format Format of the values.
 */
void Diagnostics::WriteRecord(const char *format, ...)
{
    char record[RECORD_SIZE] = "@diag,";
    const size_t prefixLength = 6;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(record + prefixLength, sizeof(record) - prefixLength, format, arguments);
    va_end(arguments);
    ESP_LOGI(COMPONENT_NAME, "%s", record);
}

/**
 * @brief Name of a task state, for the records.
 *
 * @param state Task state.
 * @return Its name.
 */
const char *Diagnostics::TaskStateName(const eTaskState state)
{
    switch (state)
    {
        case eTaskState::eRunning:
            return ("Running");
        case eTaskState::eReady:
            return ("Ready");
        case eTaskState::eBlocked:
            return ("Blocked");
        case eTaskState::eSuspended:
            return ("Suspended");
        case eTaskState::eDeleted:
            return ("Deleted");
        default:
            return ("Unknown");
    }
}

/**
 * @brief Write a record for each task and one for the load on the cores.
 *
 * task,sequence,number,name,core,state,priority,base priority,stack high-water mark,run time
 * since the last snapshot (us),CPU % of one core.  The core is -1 for a task that can run on
 * either.  The run time and percentage are 0 in a build without the FreeRTOS run-time
 * statistics.
 *
 * cpu,sequence,interval (us),load on each core (%), worked out from the time its idle task
 * ran.
 *
 * @param sequence Sequence number of the snapshot.
 */
void Diagnostics::WriteTasks(const uint32_t sequence)
{
    //
    //  The array is sized from the number of tasks now, but uxTaskGetSystemState is called
    //  afterwards and fills nothing at all if a task has been created in between and the
    //  array is too small.  Leave room for a few more and use the count it returns.
    //
    const UBaseType_t capacity = uxTaskGetNumberOfTasks() + TASK_ARRAY_HEADROOM;
    TaskStatus_t *tasks = static_cast<TaskStatus_t *>(pvPortMalloc(capacity * sizeof(TaskStatus_t)));
    if (tasks == nullptr)
    {
        ESP_LOGE(COMPONENT_NAME, "Cannot allocate memory for task information.");
        return;
    }

    configRUN_TIME_COUNTER_TYPE totalRunTime = 0;
    const UBaseType_t numberOfTasks = uxTaskGetSystemState(tasks, capacity, &totalRunTime);
    //
    //  The run time and the percentages cover the time since the last report, so they say
    //  what the system has been doing recently rather than averaged since boot.  Each core
    //  runs for the whole interval, so the percentages of one core add up to about 100
    //  times the number of cores.
    //
    const configRUN_TIME_COUNTER_TYPE interval = totalRunTime - _previousTotalRunTime;
    uint32_t idlePercentage[configNUMBER_OF_CORES] = {};
    for (UBaseType_t index = 0; index < numberOfTasks; index++)
    {
        const configRUN_TIME_COUNTER_TYPE runTime = RunTimeSinceLastReport(tasks[index].xTaskNumber, tasks[index].ulRunTimeCounter);
        const uint32_t runTimePercentage = PercentageOfInterval(runTime, interval);
        for (BaseType_t core = 0; core < configNUMBER_OF_CORES; core++)
        {
            if (tasks[index].xHandle == xTaskGetIdleTaskHandleForCore(core))
            {
                idlePercentage[core] = runTimePercentage;
            }
        }
        const int32_t core = (tasks[index].xCoreID == tskNO_AFFINITY) ? -1 : static_cast<int32_t>(tasks[index].xCoreID);
        WriteRecord("task,%" PRIu32 ",%" PRIu32 ",%s,%" PRId32 ",%s,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32, sequence, static_cast<uint32_t>(tasks[index].xTaskNumber), tasks[index].pcTaskName, core, TaskStateName(tasks[index].eCurrentState), static_cast<uint32_t>(tasks[index].uxCurrentPriority),
                    static_cast<uint32_t>(tasks[index].uxBasePriority), static_cast<uint32_t>(tasks[index].usStackHighWaterMark), static_cast<uint32_t>(runTime), runTimePercentage);
    }
    RememberRunTimes(tasks, numberOfTasks, totalRunTime);
    Utils::DeleteAndNullifyPointer(tasks);

    const uint32_t load0 = (idlePercentage[0] < 100) ? (100 - idlePercentage[0]) : 0;
#if (configNUMBER_OF_CORES > 1)
    const uint32_t load1 = (idlePercentage[1] < 100) ? (100 - idlePercentage[1]) : 0;
    WriteRecord("cpu,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32, sequence, static_cast<uint32_t>(interval), load0, load1);
#else
    WriteRecord("cpu,%" PRIu32 ",%" PRIu32 ",%" PRIu32, sequence, static_cast<uint32_t>(interval), load0);
#endif
}

/**
 * @brief Write a record for each kind of heap memory.
 *
 * Internal memory (all of the on-chip RAM the heap uses, the IRAM heap included) and DMA
 * capable memory (the data RAM) are what run out first on this part: WiFi, Bluetooth and the
 * SPI DMA buffers all need them.  Without PSRAM, 8-bit capable memory is the same as DMA
 * capable memory and 32-bit capable memory the same as internal memory, so those are not
 * reported as well.
 *
 * @param sequence Sequence number of the snapshot.
 */
void Diagnostics::WriteHeap(const uint32_t sequence)
{
    WriteHeapRegion(sequence, "internal", MALLOC_CAP_INTERNAL);
    WriteHeapRegion(sequence, "dma", MALLOC_CAP_DMA);
#if defined(CONFIG_SPIRAM)
    WriteHeapRegion(sequence, "spiram", MALLOC_CAP_SPIRAM);
#endif
}

/**
 * @brief Write the record for one kind of heap memory.
 *
 * heap,sequence,name,total,free,lowest free since boot,largest free block,free blocks,
 * fragmentation (% of the free memory outside the largest block).
 *
 * @param sequence Sequence number of the snapshot.
 * @param name Name of the kind of memory.
 * @param caps Capabilities that select it.
 */
void Diagnostics::WriteHeapRegion(const uint32_t sequence, const char *name, const uint32_t caps)
{
    multi_heap_info_t information;
    heap_caps_get_info(&information, caps);
    const uint32_t total = static_cast<uint32_t>(information.total_allocated_bytes + information.total_free_bytes);
    const uint32_t free = static_cast<uint32_t>(information.total_free_bytes);
    const uint32_t largest = static_cast<uint32_t>(information.largest_free_block);
    const uint32_t fragmentation = (free > 0) ? static_cast<uint32_t>(100 - ((static_cast<uint64_t>(largest) * 100) / free)) : 0;
    WriteRecord("heap,%" PRIu32 ",%s,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32, sequence, name, total, free, static_cast<uint32_t>(information.minimum_free_bytes), largest, static_cast<uint32_t>(information.free_blocks), fragmentation);
}

/**
 * @brief Write a record of the heap each task is using (with CONFIG_HEAP_TASK_TRACKING).
 *
 * taskheap,sequence,task,alive (1) or deleted (0),current bytes,peak bytes.
 *
 * @param sequence Sequence number of the snapshot.
 */
void Diagnostics::WriteTaskHeap(const uint32_t sequence)
{
#if defined(CONFIG_HEAP_TASK_TRACKING)
    //
    //  The allocation can fail after allocating some of its three arrays, and it clears the
    //  pointers first, so freeing is safe on either path.
    //
    heap_all_tasks_stat_t allTasksStatistics;
    esp_err_t result = heap_caps_alloc_all_task_stat_arrays(&allTasksStatistics);
    if (result == ESP_OK)
    {
        result = heap_caps_get_all_task_stat(&allTasksStatistics);
    }
    if (result == ESP_OK)
    {
        for (size_t index = 0; index < allTasksStatistics.task_count; index++)
        {
            const task_stat_t &statistics = allTasksStatistics.stat_arr[index];
            WriteRecord("taskheap,%" PRIu32 ",%s,%" PRIu32 ",%" PRIu32 ",%" PRIu32, sequence, statistics.name, static_cast<uint32_t>(statistics.is_alive ? 1 : 0), static_cast<uint32_t>(statistics.overall_current_usage), static_cast<uint32_t>(statistics.overall_peak_usage));
        }
    }
    else
    {
        ESP_LOGE(COMPONENT_NAME, "Cannot read the heap use by task: %s", esp_err_to_name(result));
    }
    heap_caps_free_all_task_stat_arrays(&allTasksStatistics);
#endif
}

/**
 * @brief Write a record for each counter, gauge and histogram (with CONFIG_MEADOW_METRICS).
 *
 * counter,sequence,name,value.  gauge,sequence,name,value,highest value.
 * histogram,sequence,name,count,total (us),longest (us), then the count in each bucket.
 *
 * @param sequence Sequence number of the snapshot.
 */
void Diagnostics::WriteMetrics(const uint32_t sequence)
{
#if defined(CONFIG_MEADOW_METRICS)
    for (size_t index = 0; index < static_cast<size_t>(MetricCounter::NumberOfCounters); index++)
    {
        const MetricCounter counter = static_cast<MetricCounter>(index);
        WriteRecord("counter,%" PRIu32 ",%s,%" PRIu32, sequence, Metrics::Name(counter), Metrics::Read(counter));
    }
    for (size_t index = 0; index < static_cast<size_t>(MetricGauge::NumberOfGauges); index++)
    {
        const MetricGauge gauge = static_cast<MetricGauge>(index);
        uint32_t maximum = 0;
        const uint32_t value = Metrics::Read(gauge, maximum);
        WriteRecord("gauge,%" PRIu32 ",%s,%" PRIu32 ",%" PRIu32, sequence, Metrics::Name(gauge), value, maximum);
    }
    for (size_t index = 0; index < static_cast<size_t>(MetricHistogram::NumberOfHistograms); index++)
    {
        const MetricHistogram histogram = static_cast<MetricHistogram>(index);
        HistogramReading reading;
        Metrics::Read(histogram, reading);
        char buckets[RECORD_SIZE] = "";
        size_t used = 0;
        for (size_t bucket = 0; (bucket < Metrics::NUMBER_OF_BUCKETS) && (used < sizeof(buckets)); bucket++)
        {
            used += snprintf(buckets + used, sizeof(buckets) - used, ",%" PRIu32, reading.buckets[bucket]);
        }
        WriteRecord("histogram,%" PRIu32 ",%s,%" PRIu32 ",%" PRIu64 ",%" PRIu32 "%s", sequence, Metrics::Name(histogram), reading.count, reading.total, reading.maximum, buckets);
    }
#endif
}

/**
 * @brief Write a record for each network protocol (with CONFIG_LWIP_STATS).
 *
 * network,sequence,protocol,sent,received,dropped,out of memory,other errors.
 *
 * @param sequence Sequence number of the snapshot.
 */
void Diagnostics::WriteNetwork(const uint32_t sequence)
{
#if defined(CONFIG_LWIP_STATS)
    const struct
    {
        const char *name;
        const struct stats_proto *statistics;
    } protocols[] = {{"link", &lwip_stats.link}, {"ip", &lwip_stats.ip}, {"tcp", &lwip_stats.tcp}, {"udp", &lwip_stats.udp}};
    for (const auto &protocol : protocols)
    {
        const struct stats_proto &statistics = *protocol.statistics;
        const uint32_t errors = static_cast<uint32_t>(statistics.chkerr) + statistics.lenerr + statistics.rterr + statistics.proterr + statistics.opterr + statistics.err;
        WriteRecord("network,%" PRIu32 ",%s,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32, sequence, protocol.name, static_cast<uint32_t>(statistics.xmit), static_cast<uint32_t>(statistics.recv), static_cast<uint32_t>(statistics.drop), static_cast<uint32_t>(statistics.memerr), errors);
    }
#endif
}

/**
 * @brief Run time a task has used since the last report.
 *
 * @param taskNumber FreeRTOS task number.
 * @param runTime Run-time counter of the task now.
 * @return Run time used since the last report.
 */
configRUN_TIME_COUNTER_TYPE Diagnostics::RunTimeSinceLastReport(const UBaseType_t taskNumber, const configRUN_TIME_COUNTER_TYPE runTime)
{
    for (size_t index = 0; index < _numberOfPreviousRunTimes; index++)
    {
        if (_previousRunTimes[index].taskNumber == taskNumber)
        {
            return (runTime - _previousRunTimes[index].runTime);
        }
    }
    return (runTime);
}

/**
 * @brief Remember the run time of each task for the next report.
 *
 * Tasks that have been deleted since the last report are dropped, because only the tasks
 * read now are kept.
 *
 * @param tasks Task states read by uxTaskGetSystemState.
 * @param numberOfTasks Number of entries in tasks.
 * @param totalRunTime Total run time read by uxTaskGetSystemState.
 */
void Diagnostics::RememberRunTimes(const TaskStatus_t *tasks, const UBaseType_t numberOfTasks, const configRUN_TIME_COUNTER_TYPE totalRunTime)
{
    _numberOfPreviousRunTimes = 0;
    for (UBaseType_t index = 0; (index < numberOfTasks) && (_numberOfPreviousRunTimes < MAXIMUM_TRACKED_TASKS); index++)
    {
        _previousRunTimes[_numberOfPreviousRunTimes].taskNumber = tasks[index].xTaskNumber;
        _previousRunTimes[_numberOfPreviousRunTimes].runTime = tasks[index].ulRunTimeCounter;
        _numberOfPreviousRunTimes++;
    }
    _previousTotalRunTime = totalRunTime;
}

/**
 * @brief Percentage of one core that a run time represents over an interval.
 *
 * @param runTime Run time used in the interval.
 * @param interval Length of the interval in the same units.
 * @return Percentage, or 0 if the interval is empty.
 */
uint32_t Diagnostics::PercentageOfInterval(const configRUN_TIME_COUNTER_TYPE runTime, const configRUN_TIME_COUNTER_TYPE interval)
{
    if (interval == 0)
    {
        return (0);
    }
    return (static_cast<uint32_t>((static_cast<uint64_t>(runTime) * 100) / interval));
}
