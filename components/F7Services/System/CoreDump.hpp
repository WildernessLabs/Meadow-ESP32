/**
 * @file CoreDump.hpp
 * @author Wilderness Labs
 * @brief Core dump handling for the ESP32 application.
 * @version 0.1
 * @date 2025-08-23
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 *
 * @note This file provides the interface for core dump management and analysis.
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "Esp32Messaging.hpp"
#include "RequestHandlerBase.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"

namespace CoreDump
{
    /**
     * @brief Get the Information about the code dump.
     * 
     * @param pvMessage Pointer to the message requesting the coredump information.
     */
    void GetInformation(void * pvMessage);

    /**
     * @brief Get part (maybe all) of the core dump.
     * 
     * @param pvMessage Pointer to the message requesting the core dump fragment.
     */
    void GetFragment(void * pvMessage);

    /**
     * @brief Erase the core dump.
     * 
     * @param pvMessage Pointer to the message requesting the core dump erasure.
     */
    void Erase(void * pvMessage);
}