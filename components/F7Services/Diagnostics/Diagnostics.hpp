/**
 * @file Diagnostics.hpp
 * @author Wilderness Labs
 * @brief Provide system diagnostic tools for the ESP32 application.
 * @version 0.1
 * @date 2025-06-04
 *
 * @copyright Copyright (c) 2025
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
     * @brief Name of the component / task.
     */
    static const char *COMPONENT_NAME;

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
    static std::string TaskStateToString(eTaskState state);

    /**
     * @brief Translate core ID to string representation.
     * 
     * @param coreID Core ID to translate.
     * @return std::string String representation of the core ID.
     */
    static std::string TranslateCoreID(BaseType_t coreID);

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
