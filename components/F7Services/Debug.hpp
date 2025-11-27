/**
 * @file Debug.hpp
 * @author Mark Stevens
 * @brief Debugging utilities for MeadowComms ESP32 firmware.
 * @date 2025-06-09
 * 
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#pragma once

#include <sdkconfig.h>

/**
 * @brief Insert a debug breakpoint into the code.
 */
#define DEBUG_BREAKPOINT() asm("break 0, 0");
