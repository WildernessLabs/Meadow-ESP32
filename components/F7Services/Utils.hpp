/**
 * @file Utils.hpp
 * @author Mark Stevens
 * @brief Utility functions for the ESP32 application.
 * @date 2025-06-05
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#pragma once

#include <stdint.h>
#include <string>

namespace Utils
{
    /**
     * @brief Format a 64-bit number with commas as thousands separators.
     *
     * @param number The number to format.
     * @return std::string The formatted number as a string.
     */
    std::string NumberWithCommas(uint64_t number);

    /**
     * @brief Format a 32-bit number with commas as thousands separators.
     *
     * @param number The number to format.
     * @return std::string The formatted number as a string.
     */
    std::string NumberWithCommas(uint32_t number);

    /**
     * @brief Delete a pointer and set it to nullptr.
     *
     * @param ptr Pointer to the pointer to delete and nullify.
     */
    void DeleteAndNullifyPointer(void **ptr);
}; // namespace Utils