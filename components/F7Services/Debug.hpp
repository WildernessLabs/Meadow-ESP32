/**
 * @file Debug.hpp
 * @author Wilderness Labs
 * @brief 
 * @version 0.1
 * @date 2025-06-06
 * 
 * @copyright Copyright (c) 2025
 */

 #pragma once

#include <sdkconfig.h>

/**
 * @brief Insert a debug breakpoint into the code.
 */
#define DEBUG_BREAKPOINT()          asm("break 0, 0");
