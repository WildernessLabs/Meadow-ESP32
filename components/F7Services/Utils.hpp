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

    /**
     * @brief Reallocate memory using the ESP32's pvPortMalloc function.
     *
     * @param ptr Pointer to the memory block to reallocate.
     * @param oldSize The size of the existing memory block (if ptr is not nullptr).
     * @param newSize The new size of the memory block.
     * @return void* Pointer to the reallocated memory block, or nullptr if allocation fails.
     */
    void *Realloc(void *ptr, size_t oldSize, size_t newSize);
}; // namespace Utils