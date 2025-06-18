/**
 * @file Diagnostics.cpp
 * @author Wilderness Labs
 * @brief Provide system diagnostic tools for the ESP32 application.
 * @version 0.1
 * @date 2025-06-04
 * 
 * @copyright Copyright (c) 2025
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
static TaskHandle_t _diagnosticsTaskHandle = NULL;

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
    if (_outputBinarySemaphore == NULL)
    {
        ERROR_MESSAGE("Failed to create output binary semaphore");
    }
    else
    {
        xSemaphoreGive(_outputBinarySemaphore);
    }
    
    BaseType_t result = xTaskCreate(Thread, "Diagnostics", 4096, NULL, 5, &_diagnosticsTaskHandle);
    
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
    ESP_LOGI(COMPONENT_NAME, "Diagnostics thread started");
    
    while (true)
    {
        Diagnostics::LockOutput();
        ESP_LOGI(COMPONENT_NAME, "");
        ESP_LOGI(COMPONENT_NAME, "=== System Diagnostics ===");
        
        ReportSystemUptime();
        ReportTaskInformation();
        ESP_LOGI(COMPONENT_NAME, "");
#if defined(CONFIG_HEAP_TRACING_TOHOST) || defined(CONFIG_HEAP_TRACING_STANDALONE)
        Heap::ReportUsage();
#endif
        
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
    uint32_t uptime_seconds = (uint32_t) (uptime_us / 1000000);
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
std::string Diagnostics::TaskStateToString(eTaskState state)
{
    switch (state)
    {
        case eTaskState::eRunning:
            return("Running");
        case eTaskState::eReady:
            return("Ready");
        case eTaskState::eBlocked:
            return("Blocked");
        case eTaskState::eSuspended:
            return("Suspended");
        case eTaskState::eDeleted:
            return("Deleted");
        default:
            return("Unknown");
    }
}

/**
 * @brief Translate core ID to string representation.
 * 
 * @param coreID Core ID for a task.
 * @return std::string String representation of the core ID.
 */
std::string Diagnostics::TranslateCoreID(BaseType_t coreID)
{
    switch (coreID)
    {
        case tskNO_AFFINITY:
            return("-");
        case 0:
            return("0");
        case 1:
            return("1");
        default:
            return("?");
    }
}

/**
 * @brief Report task information.
 */
void Diagnostics::ReportTaskInformation()
{
    TaskStatus_t *tasks;
    volatile UBaseType_t numberOfTasks;
    unsigned long totalRunTime;

    numberOfTasks = uxTaskGetNumberOfTasks();
    tasks = static_cast<TaskStatus_t *>(pvPortMalloc(numberOfTasks * sizeof(TaskStatus_t)));
    if (tasks != NULL)
    {
        ESP_LOGI(COMPONENT_NAME, "Number of tasks: %d", numberOfTasks);
        ESP_LOGI(COMPONENT_NAME, "ID  Core Task Name        State     Priority Stack Watermark Run-time Percentage");
        numberOfTasks = uxTaskGetSystemState(tasks, numberOfTasks, &totalRunTime);
        totalRunTime /= 100UL;
        for (int index = 0; index < numberOfTasks; index++)
        {
            unsigned long runTimePercentage;
            if (totalRunTime > 0)
            {
                runTimePercentage = tasks[index].ulRunTimeCounter / totalRunTime;
            }
            else
            {
                runTimePercentage = 0;
            }
            std::string taskState = TaskStateToString(tasks[index].eCurrentState);
            std::string coreID = TranslateCoreID(tasks[index].xCoreID);
            ESP_LOGI(COMPONENT_NAME, "%-2d    %s  %-16s %-9s %2" PRIu32 " (%2" PRIu32 ")  %15u %8lu  %3lu%%", tasks[index].xTaskNumber, coreID.c_str(), tasks[index].pcTaskName, taskState.c_str(),
                (long unsigned int) tasks[index].uxCurrentPriority, (long unsigned int) tasks[index].uxBasePriority,
                (unsigned int) tasks[index].usStackHighWaterMark, tasks[index].ulRunTimeCounter, runTimePercentage);
        }
        vPortFree(tasks);
    }
    else
    {
        ESP_LOGI(COMPONENT_NAME, "Cannot allocate memory for task information.\n");
    }
}
