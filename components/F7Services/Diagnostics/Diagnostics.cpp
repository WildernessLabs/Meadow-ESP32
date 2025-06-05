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

#include "Diagnostics.hpp"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "Utils.hpp"

#if defined(CONFIG_HEAP_TRACING_TOHOST)
    #include "esp_heap_trace.h"
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
static const char *COMPONENT_NAME = "Diagnostics";

/**
 *  @brief Task handle for the diagnostics thread.
 */
static TaskHandle_t _diagnosticsTaskHandle = NULL;

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
    esp_log_level_set(COMPONENT_NAME, ESP_LOG_INFO);
    ESP_LOGI(COMPONENT_NAME, "Setting up diagnostics system");
    
    BaseType_t result = xTaskCreate(Thread, "Diagnostics", 4096, NULL, 5, &_diagnosticsTaskHandle);
    
    if (result != pdPASS)
    {
        ESP_LOGE(COMPONENT_NAME, "Failed to create diagnostics task");
        return;
    }
    
    ESP_LOGI(COMPONENT_NAME, "Diagnostics system setup complete");
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
        ESP_LOGI(COMPONENT_NAME, "");
        ESP_LOGI(COMPONENT_NAME, "=== System Diagnostics ===");
        
        ReportSystemUptime();
        ReportTaskInformation();
        ReportMemoryUsage();
        ReportHeapInformation();
        
        ESP_LOGI(COMPONENT_NAME, "=== End Diagnostics ===");
        ESP_LOGI(COMPONENT_NAME, "");

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
 * @brief Report memory usage information.
 */
void Diagnostics::ReportMemoryUsage()
{
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);

    ESP_LOGI(COMPONENT_NAME, "Total heap: %s bytes", Utils::NumberWithCommas((uint64_t) (heap_info.total_allocated_bytes + heap_info.total_free_bytes)).c_str());
    ESP_LOGI(COMPONENT_NAME, "Free heap: %s bytes", Utils::NumberWithCommas((uint64_t) heap_info.total_free_bytes).c_str());
    ESP_LOGI(COMPONENT_NAME, "Allocated heap: %s bytes", Utils::NumberWithCommas((uint64_t) heap_info.total_allocated_bytes).c_str());
    ESP_LOGI(COMPONENT_NAME, "Largest free block: %s bytes", Utils::NumberWithCommas((uint64_t) heap_info.largest_free_block).c_str());
    ESP_LOGI(COMPONENT_NAME, "Minimum free heap: %s bytes", Utils::NumberWithCommas((uint64_t) esp_get_minimum_free_heap_size()).c_str());
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
        ESP_LOGI(COMPONENT_NAME, "ID  Core Task Name                        State     Priority Stack Watermark Run-time Percentage");
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
            ESP_LOGI(COMPONENT_NAME, "%-2d    %1" PRIu32 "  %-32s %-9s %2" PRIu32 " (%2" PRIu32 ")  %15u %8lu  %3lu%%", tasks[index].xTaskNumber, (uint32_t) tasks[index].xCoreID, tasks[index].pcTaskName, taskState.c_str(),
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

/**
 * @brief Report heap information including any heap tracing data.
 */
void Diagnostics::ReportHeapInformation()
{
#if defined(CONFIG_HEAP_TRACING_TOHOST)
    ESP_LOGI(COMPONENT_NAME, "Heap tracing is enabled");
    
    // Report heap trace information if available
    size_t num_totals;
    size_t num_allocations;
    size_t num_frees;
    
    heap_trace_get_count(&num_totals, &num_allocations, &num_frees);
    ESP_LOGI(COMPONENT_NAME, "Heap trace - Total: %" PRIu32 ", Allocations: %" PRIu32 ", Frees: %" PRIu32, (uint32_t) num_totals, (uint32_t) num_allocations, (uint32_t) num_frees);
#else
    ESP_LOGI(COMPONENT_NAME, "Heap tracing is not enabled");
#endif
    
    // Report additional heap capabilities
    size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t iram_free = heap_caps_get_free_size(MALLOC_CAP_32BIT);

    ESP_LOGI(COMPONENT_NAME, "DRAM free: %s bytes", Utils::NumberWithCommas((uint64_t) dram_free).c_str());
    ESP_LOGI(COMPONENT_NAME, "IRAM free: %s bytes", Utils::NumberWithCommas((uint64_t) iram_free).c_str());
}