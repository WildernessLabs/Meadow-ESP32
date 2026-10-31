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

    BaseType_t result = xTaskCreate(Thread, "Diagnostics", TASK_STACK_SIZE, nullptr, 5, &_diagnosticsTaskHandle);

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
        totalRunTime /= 100UL;
        ESP_LOGI(COMPONENT_NAME, "Number of tasks: %" PRIu32, static_cast<uint32_t>(numberOfTasks));
        ESP_LOGI(COMPONENT_NAME, "ID  Core Task Name        State     Priority Stack Watermark Run-time Percentage");
        for (UBaseType_t index = 0; index < numberOfTasks; index++)
        {
            uint32_t runTimePercentage;
            if (totalRunTime > 0)
            {
                runTimePercentage = static_cast<uint32_t>(tasks[index].ulRunTimeCounter / totalRunTime);
            }
            else
            {
                runTimePercentage = 0;
            }
            std::string taskState = TaskStateToString(tasks[index].eCurrentState);
            std::string coreID = TranslateCoreID(tasks[index].xCoreID);
            ESP_LOGI(COMPONENT_NAME, "%-2" PRIu32 "    %s  %-16s %-9s %2" PRIu32 " (%2" PRIu32 ")  %15" PRIu32 " %8" PRIu32 "  %3" PRIu32 "%%", static_cast<uint32_t>(tasks[index].xTaskNumber), coreID.c_str(), tasks[index].pcTaskName, taskState.c_str(), static_cast<uint32_t>(tasks[index].uxCurrentPriority),
                     static_cast<uint32_t>(tasks[index].uxBasePriority), static_cast<uint32_t>(tasks[index].usStackHighWaterMark), static_cast<uint32_t>(tasks[index].ulRunTimeCounter), runTimePercentage);
        }
        Utils::DeleteAndNullifyPointer(tasks);
    }
    else
    {
        ESP_LOGI(COMPONENT_NAME, "Cannot allocate memory for task information.");
    }
}
