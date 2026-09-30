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

#include "freertos/FreeRTOS.h"

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
     * @brief Release heap storage obtained from pvPortMalloc and set the caller's pointer to nullptr.
     *
     * Taking the pointer by reference means callers pass the pointer itself rather than
     * reinterpret_cast<void **>(&pointer).  Writing nullptr through a void ** into a T * is
     * undefined behaviour under strict aliasing, and the cast accepted any address at all.
     *
     * @note This releases with vPortFree, so it is only for storage from pvPortMalloc.
     *
     * @tparam T Type the pointer points to; it may be const.
     * @param pointer Pointer to release, set to nullptr on return.  A nullptr is ignored.
     */
    template<typename T> void DeleteAndNullifyPointer(T *&pointer)
    {
        if (pointer != nullptr)
        {
            vPortFree(const_cast<void *>(static_cast<const void *>(pointer)));
            pointer = nullptr;
        }
    }

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