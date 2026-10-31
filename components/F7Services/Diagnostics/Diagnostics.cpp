/**
 * @file Diagnostics.cpp
 * @author Mark Stevens
 * @brief Provide system diagnostic tools for the ESP32 application.
 * @date 2025-06-04
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 *
 * @note The standard method of sending diagnostic information to the serial output is via
 *       the trace, debug, and error macros defined in Logging.hpp.  In general this is used
 *       except where heap information is being generated.  In these cases the ESP_LOG?
 *       macros are used as they do not need the --tracemessages flag to be used in order
 *       to output the heap information.
 */

#include "sdkconfig.h"

#include <inttypes.h>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "Diagnostics.hpp"
#include "Utils.hpp"
#include "Heap.hpp"
#include "Debug.hpp"
#include "Logging.hpp"
#include "Utils.hpp"

#if defined(CONFIG_HEAP_TRACING_TOHOST) || defined(CONFIG_HEAP_TRACING_STANDALONE)
    #include "esp_heap_trace.h"
#endif

#if !defined(CONFIG_DIAGNOSTICS_INTERVAL_MS)
    #define CONFIG_DIAGNOSTICS_INTERVAL_MS 30000
#endif

/*
 * ---------------------------------------------------------------------------
 *
 *                     Initialise static data.
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component / task.
 */
const char *Diagnostics::COMPONENT_NAME = "Diagnostics";

/**
 *  @brief Task handle for the diagnostics thread.
 */
static TaskHandle_t _diagnosticsTaskHandle = nullptr;

/**
 *  @brief Lock for controlling output.
 */
SemaphoreHandle_t Diagnostics::_outputBinarySemaphore = nullptr;

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

/*
 * ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Setup the diagnostics system.
 *
 * This method initializes the diagnostics system and starts the diagnostics thread.
 */
void Diagnostics::Setup()
{
    TRACE_MESSAGE("Setup: Enter");

    _outputBinarySemaphore = xSemaphoreCreateBinary();
    if (_outputBinarySemaphore == nullptr)
    {
        ERROR_MESSAGE("Failed to create output binary semaphore");
    }
    else
    {
        xSemaphoreGive(_outputBinarySemaphore);
    }

    BaseType_t result = xTaskCreate(Thread, "Diagnostics", TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &_diagnosticsTaskHandle);

    if (result != pdPASS)
    {
        ERROR_MESSAGE("Failed to create diagnostics task");
        return;
    }

    TRACE_MESSAGE("Setup: Exit");
}

/**
 * @brief Lock the output semaphore.
 *
 * This method acquires the output semaphore, stopping this class from using the output stream.
 */
void Diagnostics::LockOutput()
{
    if (_outputBinarySemaphore == nullptr)
    {
        _outputBinarySemaphore = xSemaphoreCreateBinary();
        if (_outputBinarySemaphore == nullptr)
        {
            ERROR_MESSAGE("Failed to create output binary semaphore");
            return;
        }
        xSemaphoreGive(_outputBinarySemaphore);
    }
    if (xSemaphoreTake(_outputBinarySemaphore, portMAX_DELAY) != pdTRUE)
    {
        ERROR_MESSAGE("Failed to take output binary semaphore");
    }
}

/**
 * @brief Unlock the output semaphore.
 *
 * This method releases the output semaphore, allowing this class to use the output stream.
 */
void Diagnostics::UnlockOutput()
{
    if (_outputBinarySemaphore != nullptr)
    {
        xSemaphoreGive(_outputBinarySemaphore);
    }
}

/**
 * @brief Main diagnostics thread function.
 *
 * This method runs continuously and collects system diagnostic information
 * at regular intervals.
 *
 * @param pvParameters
 *      Pointer to task parameters (not used).
 */
void Diagnostics::Thread(void *pvParameters)
{
    TRACE_MESSAGE("Thread: Enter");

    while (true)
    {
        Diagnostics::LockOutput();
        ESP_LOGI(COMPONENT_NAME, "");
        ESP_LOGI(COMPONENT_NAME, "=== System Diagnostics ===");

        ReportSystemUptime();
        ReportTaskInformation();
        ESP_LOGI(COMPONENT_NAME, "");

        Heap::ReportUsage();
#if defined(CONFIG_HEAP_TASK_TRACKING)
        Heap::ReportHeapUseByTask();
#endif
        ESP_LOGI(COMPONENT_NAME, "");

        ESP_LOGI(COMPONENT_NAME, "=== End Diagnostics ===");
        ESP_LOGI(COMPONENT_NAME, "");
        Diagnostics::UnlockOutput();

        vTaskDelay(pdMS_TO_TICKS(CONFIG_DIAGNOSTICS_INTERVAL_MS));
    }
}

/**
 * @brief Report system uptime information.
 */
void Diagnostics::ReportSystemUptime()
{
    int64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_seconds = static_cast<uint32_t>((uptime_us / 1000000));
    uint32_t hours = uptime_seconds / 3600;
    uint32_t minutes = (uptime_seconds % 3600) / 60;
    uint32_t seconds = uptime_seconds % 60;

    ESP_LOGI(COMPONENT_NAME, "System uptime: %" PRIu32 ":%02" PRIu32 ":%02" PRIu32 " (%" PRIu32 " seconds)", hours, minutes, seconds, uptime_seconds);
}

/**
 * @brief Convert the task state enum into a string.
 *
 * @param state Task state.
 * @return string String representation of the task state.
 */
std::string Diagnostics::TaskStateToString(const eTaskState state)
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
 * @brief Translate core ID to string representation.
 *
 * @param coreID Core ID for a task.
 * @return std::string String representation of the core ID.
 */
std::string Diagnostics::TranslateCoreID(const BaseType_t coreID)
{
    switch (coreID)
    {
        case tskNO_AFFINITY:
            return ("-");
        case 0:
            return ("0");
        case 1:
            return ("1");
        default:
            return ("?");
    }
}

/**
 * @brief Report task information.
 */
void Diagnostics::ReportTaskInformation()
{
    //
    //  The array is sized from the number of tasks now, but uxTaskGetSystemState is called
    //  afterwards and fills nothing at all if a task has been created in between and the
    //  array is too small.  Leave room for a few more and use the count it returns.
    //
    const UBaseType_t capacity = uxTaskGetNumberOfTasks() + TASK_ARRAY_HEADROOM;
    TaskStatus_t *tasks = static_cast<TaskStatus_t *>(pvPortMalloc(capacity * sizeof(TaskStatus_t)));
    if (tasks != nullptr)
    {
        configRUN_TIME_COUNTER_TYPE totalRunTime = 0;
        const UBaseType_t numberOfTasks = uxTaskGetSystemState(tasks, capacity, &totalRunTime);
        //
        //  The run time and the percentages cover the time since the last report, so they say
        //  what the system has been doing recently rather than averaged since boot.  Each core
        //  runs for the whole interval, so the percentages of one core add up to about 100
        //  times the number of cores.
        //
        const configRUN_TIME_COUNTER_TYPE interval = totalRunTime - _previousTotalRunTime;
        ESP_LOGI(COMPONENT_NAME, "Number of tasks: %" PRIu32 ", interval %" PRIu32 " ms", static_cast<uint32_t>(numberOfTasks), static_cast<uint32_t>(interval / 1000));
        ESP_LOGI(COMPONENT_NAME, "ID  Core Task Name        State     Priority Stack Watermark Run-time (us)  CPU %%");
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
            std::string taskState = TaskStateToString(tasks[index].eCurrentState);
            std::string coreID = TranslateCoreID(tasks[index].xCoreID);
            ESP_LOGI(COMPONENT_NAME, "%-2" PRIu32 "    %s  %-16s %-9s %2" PRIu32 " (%2" PRIu32 ")  %15" PRIu32 " %13" PRIu32 "  %5" PRIu32, static_cast<uint32_t>(tasks[index].xTaskNumber), coreID.c_str(), tasks[index].pcTaskName, taskState.c_str(), static_cast<uint32_t>(tasks[index].uxCurrentPriority),
                     static_cast<uint32_t>(tasks[index].uxBasePriority), static_cast<uint32_t>(tasks[index].usStackHighWaterMark), static_cast<uint32_t>(runTime), runTimePercentage);
        }
        for (BaseType_t core = 0; core < configNUMBER_OF_CORES; core++)
        {
            const uint32_t load = (idlePercentage[core] < 100) ? (100 - idlePercentage[core]) : 0;
            ESP_LOGI(COMPONENT_NAME, "Core %" PRIu32 " load: %" PRIu32 "%%", static_cast<uint32_t>(core), load);
        }
        RememberRunTimes(tasks, numberOfTasks, totalRunTime);
        Utils::DeleteAndNullifyPointer(tasks);
    }
    else
    {
        ESP_LOGI(COMPONENT_NAME, "Cannot allocate memory for task information.");
    }
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
