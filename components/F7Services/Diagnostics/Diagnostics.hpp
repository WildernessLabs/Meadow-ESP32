/**
 * @file Diagnostics.hpp
 * @author Mark Stevens
 * @brief Provide system diagnostic tools for the ESP32 application.
 * @date 2025-06-04
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#pragma once

#include <sdkconfig.h>

#include <stddef.h>
#include <stdint.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/**
 * @brief Takes diagnostic snapshots of the system and writes them as records.
 *
 * A snapshot describes the tasks (with the CPU time each has used since the last snapshot),
 * the load on each core, the heap by kind of memory, the metrics, and the network statistics
 * when lwIP keeps them.  Each part is written as one line, a record, starting @diag and
 * holding comma separated values, so the records can be picked out of the log and turned
 * into tables by tools/diagnostics/parse_diag.py.  All the records of one snapshot carry the
 * same sequence number.
 *
 * Snapshots are taken when asked for, or every CONFIG_MEADOW_DIAGNOSTICS_PERIOD_MS by a low
 * priority task.
 */
class Diagnostics
{
private:
    /**
     *  @brief Stack size for the snapshot task.
     */
    static constexpr uint32_t TASK_STACK_SIZE = 4096;

    /**
     *  @brief Priority of the snapshot task.
     *
     *  Just above idle, so that taking a snapshot does not displace the tasks being measured.
     */
    static constexpr UBaseType_t TASK_PRIORITY = tskIDLE_PRIORITY + 1;

    /**
     *  @brief Shortest period between snapshots, in milliseconds.
     */
    static constexpr uint32_t MINIMUM_PERIOD_MS = 1000;

    /**
     *  @brief Extra entries allocated in the task array, for tasks created between counting
     *         the tasks and reading their state.
     */
    static constexpr UBaseType_t TASK_ARRAY_HEADROOM = 4;

    /**
     *  @brief Number of tasks whose run time is remembered from one report to the next.
     *
     *  A task beyond this many is reported with its run time since boot rather than since
     *  the last report.
     */
    static constexpr size_t MAXIMUM_TRACKED_TASKS = 48;

    /**
     *  @brief Run time a task had used when the last report was made.
     */
    struct RunTimeSample
    {
        /**
         *  @brief FreeRTOS task number, unique for the life of the application.
         */
        UBaseType_t taskNumber;

        /**
         *  @brief Run-time counter of the task when the sample was taken.
         */
        configRUN_TIME_COUNTER_TYPE runTime;
    };

    /**
     *  @brief Run time of each task at the last report.
     */
    static RunTimeSample _previousRunTimes[MAXIMUM_TRACKED_TASKS];

    /**
     *  @brief Number of entries in _previousRunTimes that are in use.
     */
    static size_t _numberOfPreviousRunTimes;

    /**
     *  @brief Total run time (time since boot) at the last report.
     */
    static configRUN_TIME_COUNTER_TYPE _previousTotalRunTime;

    /**
     *  @brief Sequence number of the last snapshot taken, 0 before the first.
     */
    static uint32_t _sequence;

    /**
     *  @brief The snapshot task, nullptr until Setup has made it.
     */
    static TaskHandle_t _taskHandle;

    /**
     *  @brief Period between snapshots in milliseconds, 0 for snapshots only when asked for.
     */
    static uint32_t _period;

    /**
     * @brief Run time a task has used since the last report.
     *
     * The counters are unsigned, so the difference is right even when a counter has wrapped
     * between the two reports.  A task that was not in the last report has used all of its
     * run time since then.
     *
     * @param taskNumber FreeRTOS task number.
     * @param runTime Run-time counter of the task now.
     * @return Run time used since the last report.
     */
    static configRUN_TIME_COUNTER_TYPE RunTimeSinceLastReport(const UBaseType_t taskNumber, const configRUN_TIME_COUNTER_TYPE runTime);

    /**
     * @brief Remember the run time of each task for the next report.
     *
     * @param tasks Task states read by uxTaskGetSystemState.
     * @param numberOfTasks Number of entries in tasks.
     * @param totalRunTime Total run time read by uxTaskGetSystemState.
     */
    static void RememberRunTimes(const TaskStatus_t *tasks, const UBaseType_t numberOfTasks, const configRUN_TIME_COUNTER_TYPE totalRunTime);

    /**
     * @brief Percentage of one core that a run time represents over an interval.
     *
     * @param runTime Run time used in the interval.
     * @param interval Length of the interval in the same units.
     * @return Percentage, or 0 if the interval is empty.
     */
    static uint32_t PercentageOfInterval(const configRUN_TIME_COUNTER_TYPE runTime, const configRUN_TIME_COUNTER_TYPE interval);

    /**
     * @brief Gives the tests access to the private members.
     */
    friend class TestDiagnostics;

    /**
     * @brief Mutex that stops two snapshots being written at once.
     *
     * @return The mutex.
     */
    static SemaphoreHandle_t SnapshotMutex();

    /**
     * @brief The snapshot task: takes a snapshot each period, or when asked.
     *
     * @param parameters Not used.
     */
    static void Task(void *parameters);

    /**
     * @brief Write one record: @diag, then the values formatted as for printf.
     *
     * @param format Format of the values.
     */
    static void WriteRecord(const char *format, ...) __attribute__((format(printf, 1, 2)));

    /**
     * @brief Write a record for each task and one for the load on the cores.
     *
     * @param sequence Sequence number of the snapshot.
     */
    static void WriteTasks(const uint32_t sequence);

    /**
     * @brief Write a record for each kind of heap memory.
     *
     * @param sequence Sequence number of the snapshot.
     */
    static void WriteHeap(const uint32_t sequence);

    /**
     * @brief Write the record for one kind of heap memory.
     *
     * @param sequence Sequence number of the snapshot.
     * @param name Name of the kind of memory.
     * @param caps Capabilities that select it.
     */
    static void WriteHeapRegion(const uint32_t sequence, const char *name, const uint32_t caps);

    /**
     * @brief Write a record of the heap each task is using (with CONFIG_HEAP_TASK_TRACKING).
     *
     * @param sequence Sequence number of the snapshot.
     */
    static void WriteTaskHeap(const uint32_t sequence);

    /**
     * @brief Write a record for each counter, gauge and histogram (with CONFIG_MEADOW_METRICS).
     *
     * @param sequence Sequence number of the snapshot.
     */
    static void WriteMetrics(const uint32_t sequence);

    /**
     * @brief Write a record for each network protocol (with CONFIG_LWIP_STATS).
     *
     * @param sequence Sequence number of the snapshot.
     */
    static void WriteNetwork(const uint32_t sequence);

    /**
     * @brief Name of a task state, for the records.
     *
     * @param state Task state.
     * @return Its name.
     */
    static const char *TaskStateName(const eTaskState state);

public:
    /**
     * @brief Name of the component; the records are logged under it.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Largest record, in characters, including the @diag prefix.  A longer one is cut.
     */
    static constexpr size_t RECORD_SIZE = 256;

    /**
     * @brief Start the snapshot task.  Calling it again does nothing.
     *
     * @param periodMilliseconds Time between snapshots, or 0 to take them only when asked for
     *                           with RequestSnapshot.  A period below one second is raised to
     *                           one second.
     */
    static void Setup(const uint32_t periodMilliseconds);

    /**
     * @brief Ask the snapshot task to take a snapshot now.  Does nothing before Setup.
     */
    static void RequestSnapshot();

    /**
     * @brief Take a snapshot now, in the calling task, and write its records.
     *
     * @return Sequence number of the snapshot.
     */
    static uint32_t TakeSnapshot();

    /**
     * @brief Sequence number of the last snapshot taken, 0 before the first.
     *
     * @return The sequence number.
     */
    static uint32_t LastSequence();
};
