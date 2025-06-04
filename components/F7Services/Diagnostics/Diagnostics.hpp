/**
 * @file Diagnostics.hpp
 * @author Wilderness Labs
 * @brief Provide system diagnostic tools for the ESP32 application.
 * @version 0.1
 * @date 2025-06-04
 * 
 * @copyright Copyright (c) 2025
 */
#ifndef DIAGNOSTICS_HPP
#define DIAGNOSTICS_HPP

#include <stdint.h>

class Diagnostics
{
private:
    /**
     * @brief Report system uptime information.
     */
    static void ReportSystemUptime();
    
    /**
     * @brief Report memory usage information.
     */
    static void ReportMemoryUsage();
    
    /**
     * @brief Report task information.
     */
    static void ReportTaskInformation();
    
    /**
     * @brief Report heap information including any heap tracing data.
     */
    static void ReportHeapInformation();

    /**
     * @brief FreeRTOS task function for the diagnostics thread.
     */
    static void Thread(void *pvParameters);

public:
    static void Setup();
};

#endif // DIAGNOSTICS_HPP
