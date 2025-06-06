/**
 * @file Utils.hpp
 * @author Wilderness Labs
 * @brief Utility functions for the ESP32 application.
 * @version 0.1
 * @date 2025-06-05
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#ifndef UTILS_HPP
    #define UTILS_HPP

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
}; // namespace Utils

#endif // UTILS_HPP