/**
 * @file Globals.hpp
 * @author Wilderness Labs (Mark Stevens)
 * @brief Provide a home for global variables.
 * @version 0.1
 * @date 2024-12-12
 *
 * @copyright Copyright (c) 2024
 */
#ifndef __GLOBALS_HPP__
#define __GLOBALS_HPP__

#include "freertos/task.h"

/**
 * @brief Task priorities.
 */

/**
 * @brief Bluetooth task priority.
 */
#define PRIORITY_BLUETOOTH_TASK ((configMAX_PRIORITIES - 1) >> 2)

#endif // __GLOBALS_HPP__