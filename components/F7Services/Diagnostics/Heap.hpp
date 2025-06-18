/**
 * @file Heap.hpp
 * @author Wilderness Labs
 * @brief Heap memory diagnostics for the ESP32 application.
 * @version 0.1
 * @date 2025-06-16
 * 
 * @copyright Copyright (c) 2025
 */
#pragma once

#include "sdkconfig.h"

#include <stdint.h>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#if defined(CONFIG_HEAP_TRACING_TOHOST) || defined(CONFIG_HEAP_TRACING_STANDALONE)
    #include "esp_heap_trace.h"
#endif

/**
 * @brief Class providing heap memory diagnostics and management functionality.
 */
class Heap
{
public:
    /**
     * @brief Name of the FreeRTOS component / task for the heap diagnostics.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Initialize the heap diagnostic subsystem.
     */
    static void Setup();
    
    /**
     * @brief Report comprehensive heap information.
     */
    static void ReportUsage();
    
    /**
     * @brief Check heap integrity and report any issues.
     * 
     * @return true if heap is valid, false if corruption detected
     */
    static bool CheckIntegrity();

    /**
     * @brief Hook for memory allocation events.
     * 
     * @param ptr Pointer to the allocated memory.
     * @param size Size of the allocated memory in bytes.
     * @param caps Memory capabilities flags.
     */
    static void AllocationHook(const void * const ptr, const size_t size, const uint32_t caps);

    /**
     * @brief Hook for memory deallocation events.
     * 
     * @param ptr Pointer to the memory being freed.
     */
    static void FreeHook(const void * const ptr);

#if defined(CONFIG_HEAP_TRACING_STANDALONE)
    /**
     * @brief Start heap tracing if supported by the ESP32 configuration.
     * 
     * @param mode The heap trace mode, use HEAP_TRACE_ALL or HEAP_TRACE_LEAKS (default)
     */
    static void StartTracing(heap_trace_mode_t mode = HEAP_TRACE_LEAKS);
    
    /**
     * @brief Stop heap tracing and report results.
     */
    static void StopTracing();

    /**
     * @brief Dump all heap trace records to stdout.
     */
    static void DumpTraceRecords();
    
    /**
     * @brief Report current heap tracing statistics.
     */
    static void ReportStatistics();
#endif

private:
#if defined(CONFIG_HEAP_USE_HOOKS)

    /**
     * @brief Array to hold pointers to allocated memory for tracking.
     */
    static const void *_allocations[CONFIG_HOOK_REGISTER_SIZE];

    /**
     * @brief Count of current allocations.
     */
    static uint32_t _allocationCount;

#endif // CONFIG_HEAP_USE_HOOKS

#if defined (CONFIG_HEAP_TRACING_STANDALONE)

    /**
     * @brief Pointer to the heap trace records.
     */
    static heap_trace_record_t *_traceRecords;

#endif
};
