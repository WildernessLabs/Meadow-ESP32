/**
 * @file Heap.cpp
 * @author Wilderness Labs
 * @brief Heap memory diagnostics for the ESP32 application.
 * @version 0.1
 * @date 2025-06-16
 *
 * @copyright Copyright (c) 2025
 */

#include "sdkconfig.h"

#include <inttypes.h>
#include <string>

#include "Heap.hpp"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#if defined(CONFIG_HEAP_TRACING_DEST)
#include "esp_heap_trace.h"
#endif

#include "Utils.hpp"
#include "Heap.hpp"
#include "Debug.hpp"

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

#if defined(CONFIG_HEAP_USE_HOOKS)

/**
 * @brief Number of allocations tracked by the heap hooks.
 */
uint32_t Heap::_allocationCount = 0;

/**
 * @brief Array to hold pointers to allocated memory for tracking.
 */
const void *Heap::_allocations[CONFIG_HOOK_REGISTER_SIZE] = {nullptr};

#endif

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
    esp_log_level_set(COMPONENT_NAME, ESP_LOG_INFO);

    ESP_LOGI(COMPONENT_NAME, "Setting up heap diagnostics");

    ESP_LOGI(COMPONENT_NAME, "Heap diagnostics setup complete");
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
    ESP_LOGI(COMPONENT_NAME, "Heap fragmentation: %d%%", heap_info.total_free_bytes ? (100 - (heap_info.largest_free_block * 100) / heap_info.total_free_bytes) : 0);

#if defined(CONFIG_HEAP_USE_HOOKS)
    ESP_LOGI(COMPONENT_NAME, "Current allocations: %s of %s", Utils::NumberWithCommas((uint64_t) _allocationCount).c_str(), Utils::NumberWithCommas((uint64_t) CONFIG_HOOK_REGISTER_SIZE).c_str());
#endif
}

/**
 * @brief Check heap integrity and report any issues.
 *
 * @return true if heap is valid, false if corruption detected
 */
bool Heap::CheckIntegrity()
{
    ESP_LOGI(COMPONENT_NAME, "Checking heap integrity...");

    bool integrity = heap_caps_check_integrity_all(true);

    if (integrity)
    {
        ESP_LOGI(COMPONENT_NAME, "Heap integrity check passed");
    }
    else
    {
        ESP_LOGE(COMPONENT_NAME, "HEAP CORRUPTION DETECTED!");
    }

    return integrity;
}

#if defined(CONFIG_HEAP_USE_HOOKS)

/**
 * @brief Hook for memory allocation events.
 *
 * This is defined in the ESP-IDF heap tracing API.
 *
 * @param ptr Pointer to the allocated memory.
 * @param size Size of the allocated memory in bytes.
 * @param caps Memory capabilities flags.
 */
void esp_heap_trace_alloc_hook(void *ptr, size_t size, uint32_t caps)
{
    Heap::AllocationHook(ptr, size, caps);
}

/**
 * @brief Hook for memory deallocation events.
 *
 * This is defined in the ESP-IDF heap tracing API.
 *
 * @param ptr Pointer to the memory being freed.
 */
void esp_heap_trace_free_hook(void *ptr)
{
    Heap::FreeHook(ptr);
}

/**
 * @brief Hook for memory allocation events.
 *
 * @param ptr Pointer to the allocated memory.
 * @param size Size of the allocated memory in bytes.
 * @param caps Memory capabilities flags.
 */
void Heap::AllocationHook(const void * const ptr, const size_t size, const uint32_t caps)
{
    if (_allocationCount < CONFIG_HOOK_REGISTER_SIZE)
    {
        for (int index = 0; index < CONFIG_HOOK_REGISTER_SIZE; index++)
        {
            if (_allocations[index] == nullptr)
            {
                _allocations[index] = ptr;
                _allocationCount++;
                break;
            }
        }
    }
    else
    {
        DEBUG_BREAKPOINT();
    }
}

/**
 * @brief Hook for memory deallocation events.
 *
 * @param ptr Pointer to the memory being freed.
 */
void Heap::FreeHook(const void * const ptr)
{
    bool found = false;

    for (int index = 0; index < CONFIG_HOOK_REGISTER_SIZE; index++)
    {
        if (_allocations[index] == ptr)
        {
            _allocations[index] = nullptr;
            _allocationCount--;
            found = true;
            break;
        }
    }

    if (!found)
    {
        DEBUG_BREAKPOINT();
    }
}

#endif // CONFIG_HEAP_USE_HOOKS

#if defined(CONFIG_HEAP_TRACING_TOHOST)
/**
 * @brief Start heap tracing if supported by the ESP32 configuration.
 */
void Heap::StartTracing()
{
    ESP_LOGI(COMPONENT_NAME, "Starting heap tracing");

    esp_err_t result = heap_trace_start(HEAP_TRACE_LEAKS);
    if (result == ESP_OK)
    {
        ESP_LOGI(COMPONENT_NAME, "Heap tracing started successfully");
    }
    else
    {
        ESP_LOGE(COMPONENT_NAME, "Failed to start heap tracing: %d", result);
    }
}

/**
 * @brief Stop heap tracing and report results.
 */
void Heap::StopTracing()
{
    ESP_LOGI(COMPONENT_NAME, "Stopping heap tracing");

    esp_err_t result = heap_trace_stop();
    if (result == ESP_OK)
    {
        ESP_LOGI(COMPONENT_NAME, "Heap tracing stopped successfully");

        // Report final results
        ReportTracingStatus();
    }
    else
    {
        ESP_LOGE(COMPONENT_NAME, "Failed to stop heap tracing: %d", result);
    }
}

/**
 * @brief Report current heap tracing statistics.
 */
void Heap::ReportTracingStatus()
{
    size_t num_records;
    size_t num_allocations;
    size_t num_frees;

    heap_trace_get_count(&num_records, &num_allocations, &num_frees);

    ESP_LOGI(COMPONENT_NAME, "Heap trace statistics:");
    ESP_LOGI(COMPONENT_NAME, "  Records: %" PRIu32, (uint32_t) num_records);
    ESP_LOGI(COMPONENT_NAME, "  Allocations: %" PRIu32, (uint32_t) num_allocations);
    ESP_LOGI(COMPONENT_NAME, "  Frees: %" PRIu32, (uint32_t) num_frees);
    ESP_LOGI(COMPONENT_NAME, "  Leaks: %" PRIu32, (uint32_t) (num_allocations - num_frees));
}

#endif
