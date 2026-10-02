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
     * @brief Format an IPv4 address in dotted decimal.
     *
     *  Unlike inet_ntoa, which formats into one static buffer shared by every caller and
     *  every task, each call returns its own string, so two addresses can be formatted in
     *  one statement or on two tasks at once (issue #171).
     *
     * @param address Address in network byte order, as held in in_addr::s_addr.
     * @return std::string The address, for example "192.168.1.1".
     */
    std::string Ipv4AddressToString(const uint32_t address);

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

    /**
     * @brief Log that a FreeRTOS object could not be created, and stop.
     *
     *  Used where a semaphore, queue, event group or task the firmware cannot run without
     *  fails to be created, which in practice means memory ran out at start-up.  Stopping
     *  here, with the object named, is clearer than the FreeRTOS assertion the first use of
     *  a null handle would otherwise raise later and elsewhere (issue #272).
     *
     * @param what Description of the object that could not be created.
     */
    [[noreturn]] void CreationFailed(const char * const what);

    /**
     * @brief Check that a FreeRTOS object was created, stopping with a diagnostic if not.
     *
     * @tparam T Handle type, such as SemaphoreHandle_t, QueueHandle_t or EventGroupHandle_t.
     * @param handle Handle returned by the creation call.
     * @param what Description of the object, for the diagnostic.
     * @return The handle, which is never nullptr.
     */
    template<typename T> T RequireCreated(T handle, const char * const what)
    {
        if (handle == nullptr)
        {
            CreationFailed(what);
        }
        return (handle);
    }

    /**
     * @brief Check that xTaskCreate or xTaskCreatePinnedToCore succeeded, stopping with a diagnostic if not.
     *
     * @param result Value returned by the creation call.
     * @param what Description of the task, for the diagnostic.
     */
    void RequireTaskCreated(const BaseType_t result, const char * const what);
}; // namespace Utils