/**
 * @file Heap.cpp
 * @author Mark Stevens
 * @brief Heap memory diagnostics for the ESP32 application.
 * @date 2025-06-16
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

#include "Heap.hpp"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "esp_heap_task_info.h"
#include "esp_heap_trace.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
 * @brief Number of records standalone heap tracing keeps.
 *
 * @note CONFIG_TRACE_RECORD_NUM is defined in the F7Services Kconfig.projbuild.  It used
 *       to be defined in main, which the test project does not build, so the test build
 *       never had the symbol and a fixed count was used here instead.
 */
static const uint32_t TRACE_RECORD_COUNT = CONFIG_TRACE_RECORD_NUM;

/**
 * @brief Maximum number of heap trace records.
 */
heap_trace_record_t *Heap::_traceRecords = nullptr;

#endif // CONFIG_HEAP_TRACING_STANDALONE

/**
 * @brief Flag indicating whether heap tracing is currently active.
 */
bool Heap::_isRunning = false;

/**
 * @brief True once Setup has run, so that running it again does nothing.
 */
bool Heap::_isSetup = false;

/*
 * ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Initialize the heap diagnostic subsystem.
 *
 * With heap tracing to the host, tracing is started as soon as a SystemView capture starts,
 * so a capture shows every allocation from then on.  Setup may be called more than once (each
 * SystemRequestHandler calls it); only the first call does anything.
 */
void Heap::Setup()
{
    if (_isSetup)
    {
        return;
    }
    _isSetup = true;
    esp_log_level_set(COMPONENT_NAME, ESP_LOG_INFO);

    TRACE_MESSAGE("Setup: Enter");

#if defined(CONFIG_HEAP_TRACING_STANDALONE) || defined(CONFIG_HEAP_TRACING_TOHOST)
    esp_err_t result = ESP_FAIL;

    #if defined(CONFIG_HEAP_TRACING_STANDALONE)
    _traceRecords = static_cast<heap_trace_record_t *>(pvPortMalloc(sizeof(heap_trace_record_t) * TRACE_RECORD_COUNT));
    if (_traceRecords == nullptr)
    {
        ERROR_MESSAGE("Failed to allocate memory for heap trace records");
    }
    else
    {
        result = heap_trace_init_standalone(_traceRecords, TRACE_RECORD_COUNT);
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
    #if defined(CONFIG_HEAP_TRACING_TOHOST)
        if (xTaskCreate(HostTracingTask, "HeapTrace", HOST_TRACING_TASK_STACK_SIZE, nullptr, tskIDLE_PRIORITY + 1, nullptr) != pdPASS)
        {
            ERROR_MESSAGE("Failed to create the task that starts heap tracing to the host");
        }
    #endif
    }
#else
    TRACE_MESSAGE("Heap tracing not configured");
#endif

    TRACE_MESSAGE("Setup: Exit");
}

#if defined(CONFIG_HEAP_TRACING_TOHOST)

/**
 * @brief Start heap tracing to the host once a SystemView capture starts.
 *
 * @param parameters Not used.
 */
void Heap::HostTracingTask(void *parameters)
{
    StartTracing(HEAP_TRACE_ALL);
    vTaskDelete(nullptr);
}

#endif

/**
 * @brief Check heap integrity and report any issues.
 *
 * @return true if heap is valid, false if corruption detected
 */
bool Heap::CheckIntegrity()
{
#if defined(CONFIG_ENABLE_HEAP_INTEGRITY_CHECKS)
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
void Heap::StartTracing(const heap_trace_mode_t mode)
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
