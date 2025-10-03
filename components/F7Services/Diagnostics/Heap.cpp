/**
 * @file Heap.cpp
 * @author Wilderness Labs
 * @brief Heap memory diagnostics for the ESP32 application.
 * @version 0.1
 * @date 2025-06-16
 *
 * @copyright Copyright (c) 2025
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

#include "Heap.hpp"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "esp_heap_task_info.h"
#include "esp_heap_trace.h"

#include "Utils.hpp"
#include "Debug.hpp"
#include "Logging.hpp"

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
const char *Heap::COMPONENT_NAME = "HeapDiagnostics";

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

/**
 * @brief Maximum number of heap trace records.
 */
heap_trace_record_t *Heap::_traceRecords = nullptr;

#endif // CONFIG_HEAP_TRACING_STANDALONE

/**
 * @brief Flag indicating whether heap tracing is currently active.
 */
bool Heap::_isRunning = false;

/*
 * ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Initialize the heap diagnostic subsystem.
 */
void Heap::Setup()
{
    esp_err_t result = ESP_FAIL;

    esp_log_level_set(COMPONENT_NAME, ESP_LOG_INFO);

    TRACE_MESSAGE("Setup: Enter");

#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    _traceRecords = static_cast<heap_trace_record_t *>(pvPortMalloc(sizeof(heap_trace_record_t) * CONFIG_TRACE_RECORD_NUM));
    if (_traceRecords == NULL)
    {
        ERROR_MESSAGE("Failed to allocate memory for heap trace records");
    }
    else
    {
        result = heap_trace_init_standalone(_traceRecords, CONFIG_TRACE_RECORD_NUM);
    }
#endif
#if defined(CONFIG_HEAP_TRACING_TOHOST)
    result = heap_trace_init_tohost();
#endif
    if (result != ESP_OK)
    {
        ERROR_MESSAGE("Failed to initialize heap tracing: %s", esp_err_to_name(result));
    }
    else
    {
        TRACE_MESSAGE("Heap tracing initialized successfully");
    }

    TRACE_MESSAGE("Setup: Exit");
}

/**
 * @brief Report comprehensive heap information.
 */
void Heap::ReportUsage()
{
    ESP_LOGI(COMPONENT_NAME, "Memory Usage");
    bool valid = heap_caps_check_integrity_all(true);
    ESP_LOGI(COMPONENT_NAME, "Heap integrity check: %s", valid ? "PASS" : "FAILED");
    if (!valid)
    {
        DEBUG_BREAKPOINT();
    }

    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);

    ESP_LOGI(COMPONENT_NAME, "Total heap: %s bytes", Utils::NumberWithCommas((uint64_t) (heap_info.total_allocated_bytes + heap_info.total_free_bytes)).c_str());
    ESP_LOGI(COMPONENT_NAME, "Free heap: %s bytes", Utils::NumberWithCommas((uint64_t) heap_info.total_free_bytes).c_str());
    ESP_LOGI(COMPONENT_NAME, "Allocated heap: %s bytes", Utils::NumberWithCommas((uint64_t) heap_info.total_allocated_bytes).c_str());
    ESP_LOGI(COMPONENT_NAME, "Free blocks: %s", Utils::NumberWithCommas((uint64_t) heap_info.free_blocks).c_str());
    ESP_LOGI(COMPONENT_NAME, "Largest free block: %s bytes", Utils::NumberWithCommas((uint64_t) heap_info.largest_free_block).c_str());
    ESP_LOGI(COMPONENT_NAME, "Minimum free heap: %s bytes", Utils::NumberWithCommas((uint64_t) esp_get_minimum_free_heap_size()).c_str());
    ESP_LOGI(COMPONENT_NAME, "Heap fragmentation: %" PRIu32 "%%", heap_info.total_free_bytes ? (100 - (heap_info.largest_free_block * 100) / heap_info.total_free_bytes) : 0);
}

/**
 * @brief Check heap integrity and report any issues.
 *
 * @return true if heap is valid, false if corruption detected
 */
bool Heap::CheckIntegrity()
{
#if defined (CONFIG_ENABLE_HEAP_INTEGRITY_CHECKS)
    TRACE_MESSAGE("CheckIntegrity: Enter");

    bool integrity = heap_caps_check_integrity_all(true);
    if (!integrity)
    {
        ERROR_MESSAGE("HEAP CORRUPTION DETECTED!");
        DEBUG_BREAKPOINT();
    }

    TRACE_MESSAGE("CheckIntegrity: Exit");

    return integrity;
#else
    TRACE_MESSAGE("CheckIntegrity: Integrity checks disabled");
    return true;
#endif // CONFIG_ENABLE_HEAP_INTEGRITY_CHECKS
}

/**
 * @brief Start heap tracing if supported by the ESP32 configuration.
 *
 * @param mode The heap trace mode, use HEAP_TRACE_ALL or HEAP_TRACE_LEAKS (default)
*/
void Heap::StartTracing(heap_trace_mode_t mode)
{
    TRACE_MESSAGE("StartTracing: Enter");

    if (_isRunning)
    {
        ERROR_MESSAGE("Heap tracing is already running");
        return;
    }

#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    if (_traceRecords == nullptr)
    {
        ERROR_MESSAGE("Heap trace records not initialized");
        _isRunning = false;
        return;
    }
#endif

    esp_err_t result = heap_trace_start(mode);
    if (result == ESP_OK)
    {
        _isRunning = true;
    }
    else
    {
        ERROR_MESSAGE("Failed to start heap tracing: %s", esp_err_to_name(result));
        _isRunning = false;
    }

    TRACE_MESSAGE("StartTracing: Exit");
}

/**
 * @brief Stop heap tracing and report results.
 */
void Heap::StopTracing()
{
    TRACE_MESSAGE("StopTracing: Enter");

#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    if (!_traceRecords)
    {
        ERROR_MESSAGE("Heap trace records not initialized");
        return;
    }
#endif

    esp_err_t result = heap_trace_stop();
    if (result != ESP_OK)
    {
        ERROR_MESSAGE("Failed to stop heap tracing: %s", esp_err_to_name(result));
    }
    _isRunning = false;

    TRACE_MESSAGE("StopTracing: Exit");
}

/**
 * @brief Dump all heap trace records to stdout
 */
void Heap::DumpTraceRecords()
{
#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    if (!_traceRecords)
    {
        ERROR_MESSAGE("Heap trace records not initialized");
        return;
    }
#endif

    heap_trace_dump();
}

/**
 * @brief Report current heap tracing statistics.
 */
void Heap::ReportStatistics()
{
    TRACE_MESSAGE("ReportStatistics: Enter");

#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    if (!_traceRecords)
    {
        ERROR_MESSAGE("Heap trace records not initialized");
        return;
    }
#endif

    esp_err_t result;
    heap_trace_summary_t summary;
    result = heap_trace_summary(&summary);
    if (result != ESP_OK)
    {
        ERROR_MESSAGE("Failed to get heap trace summary: %s", esp_err_to_name(result));
        return;
    }

    ESP_LOGI(COMPONENT_NAME, "Heap trace statistics:");
    ESP_LOGI(COMPONENT_NAME, "    Records: %" PRIu32, (uint32_t) summary.count);
    ESP_LOGI(COMPONENT_NAME, "    Allocations: %" PRIu32, (uint32_t) summary.total_allocations);
    ESP_LOGI(COMPONENT_NAME, "    Frees: %" PRIu32, (uint32_t) summary.total_frees);
    ESP_LOGI(COMPONENT_NAME, "    Highwatermark: %" PRIu32, (uint32_t) summary.high_water_mark);
    ESP_LOGI(COMPONENT_NAME, "    Overflowed: %s", summary.has_overflowed ? "true" : "false");

    TRACE_MESSAGE("ReportStatistics: Exit");
}

#if defined(CONFIG_HEAP_TASK_TRACKING)

/**
 * @brief Report heap usage by task.
 *
 * This method reports the heap usage for each task in the system.
 */
void Heap::ReportHeapUseByTask()
{
    heap_all_tasks_stat_t allTasksStatistics;
    assert(heap_caps_alloc_all_task_stat_arrays(&allTasksStatistics) == ESP_OK);
    assert(heap_caps_get_all_task_stat(&allTasksStatistics) == ESP_OK);

    ESP_LOGI(COMPONENT_NAME, "--------------------------------------------------------------------------------");
    ESP_LOGI(COMPONENT_NAME, "                        Printing All Tasks Information\n");
    ESP_LOGI(COMPONENT_NAME, "--------------------------------------------------------------------------------");
    for (size_t task_idx = 0; task_idx < allTasksStatistics.task_count; task_idx++)
    {
        task_stat_t taskStatistics = allTasksStatistics.stat_arr[task_idx];
        ESP_LOGI(COMPONENT_NAME, "%s: %s: Peak Usage %" PRIu16 ", Current Usage %" PRIu16, taskStatistics.name,
                                 taskStatistics.is_alive ? "ALIVE  " : "DELETED", taskStatistics.overall_peak_usage, taskStatistics.overall_current_usage);

        for (size_t heapIndex = 0; heapIndex < taskStatistics.heap_count; heapIndex++)
        {
            heap_stat_t heapStatistics = taskStatistics.heap_stat[heapIndex];
            ESP_LOGI(COMPONENT_NAME, "    %s: Caps: %" PRIu32 ". Size %" PRIu16 ", Current Usage %" PRIu16 ", Peak Usage %" PRIu16 ", alloc count %" PRIu16, heapStatistics.name,
                                    heapStatistics.caps, heapStatistics.size, heapStatistics.current_usage, heapStatistics.peak_usage, heapStatistics.alloc_count);

            for (size_t allocationIndex = 0; allocationIndex < heapStatistics.alloc_count; allocationIndex++)
            {
                heap_task_block_t allocation = heapStatistics.alloc_stat[allocationIndex];
                ESP_LOGI(COMPONENT_NAME, "        %p: Size: %" PRIu32, allocation.address, allocation.size);
            }
        }
    }
    ESP_LOGI(COMPONENT_NAME, "--------------------------------------------------------------------------------");
    ESP_LOGI(COMPONENT_NAME, "                        End of All Tasks Information\n");
    ESP_LOGI(COMPONENT_NAME, "--------------------------------------------------------------------------------");

    heap_caps_free_all_task_stat_arrays(&allTasksStatistics);
}

#endif // CONFIG_HEAP_TASK_TRACKING
