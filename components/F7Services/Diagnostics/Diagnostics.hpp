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

#include <stdint.h>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class Diagnostics
{
private:
    /**
     *  @brief Stack size for the diagnostics task.
     */
    static constexpr uint32_t TASK_STACK_SIZE = 8192;

    /**
     *  @brief Priority of the diagnostics task.
     *
     *  Just above idle, so that taking a report does not displace the tasks being measured.
     *  It used to be 5, above several of them.
     */
    static constexpr UBaseType_t TASK_PRIORITY = tskIDLE_PRIORITY + 1;

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
     *  @brief Lock for controlling access to the members of this class.
     */
    static SemaphoreHandle_t _outputBinarySemaphore;

    /**
     * @brief Report system uptime information.
     */
    static void ReportSystemUptime();

    /**
     * @brief Report task information.
     *
     * @param state Task state.
     * @return Character representation of the task state.
     */
    static std::string TaskStateToString(const eTaskState state);

    /**
     * @brief Translate core ID to string representation.
     *
     * @param coreID Core ID to translate.
     * @return std::string String representation of the core ID.
     */
    static std::string TranslateCoreID(const BaseType_t coreID);

    /**
     * @brief Report task information.
     */
    static void ReportTaskInformation();

    /**
     * @brief FreeRTOS task function for the diagnostics thread.
     */
    static void Thread(void *pvParameters);

public:
    /**
     * @brief Name of the component / task.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Setup the diagnostics system.
     */
    static void Setup();

    /**
     * @brief Lock the output semaphore.
     */
    static void LockOutput();

    /**
     * @brief Unlock the output semaphore.
     */
    static void UnlockOutput();
};
