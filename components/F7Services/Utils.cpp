/**
 * @file Utils.cpp
 * @author Mark Stevens
 * @brief Utility functions for the ESP32 application.
 * @date 2025-06-05
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <string>

#include "Utils.hpp"
#include "Logging.hpp"

#include <stdlib.h>
#include "esp_err.h"

/**
 * @brief Format a number with commas as thousands separators.
 *
 * @param number The number to format.
 * @return std::string The formatted number.
 */
std::string Utils::NumberWithCommas(uint64_t number)
{
    uint64_t bufferSize = 27; // 20 digits, plus 6 commas (max) + terminator.
    char buffer[27];

    size_t len = snprintf(buffer, bufferSize, "%" PRIu64, number);

    if (len <= 3)
    {
        return std::string(buffer);
    }

    int commas = (len - 1) / 3;
    char temp[27];
    strcpy(temp, buffer);

    int tempIndex = len;
    int bufIndex = len + commas;
    buffer[bufIndex--] = '\0';

    int count = 0;
    while (tempIndex > 0)
    {
        buffer[bufIndex--] = temp[--tempIndex];
        count++;
        if (count == 3 && tempIndex > 0)
        {
            buffer[bufIndex--] = ',';
            count = 0;
        }
    }

    return std::string(buffer);
}

/**
 * @brief Format a number with commas as thousands separators.
 *
 * @param number The number to format.
 * @return std::string The formatted number.
 */
std::string Utils::NumberWithCommas(uint32_t number)
{
    return NumberWithCommas(static_cast<uint64_t>(number));
}

/**
 * @brief Reallocate memory using the ESP32's pvPortMalloc function.
 *
 * @note This function mimics the behavior of realloc, but uses pvPortMalloc and vPortFree for memory management.
 *       It will zero the memory block before copying the old data.
 *
 * @param ptr Pointer to the memory block to reallocate.
 * @param oldSize The size of the existing memory block (if ptr is not nullptr).
 * @param newSize The new size of the memory block.
 * @return void* Pointer to the reallocated memory block, or nullptr if allocation fails.
 */
void *Utils::Realloc(void *ptr, size_t oldSize, size_t newSize)
{
    void *result = nullptr;

    if (!ptr)
    {
        result = pvPortMalloc(newSize);
        if (result)
        {
            memset(result, 0, newSize);
        }
    }
    else
    {
        if (newSize == 0)
        {
            vPortFree(ptr);
            result = nullptr;
        }
        else
        {
            void *newPtr = pvPortMalloc(newSize);
            if (newPtr != nullptr)
            {
                memset(newPtr, 0, newSize);
                memcpy(newPtr, ptr, oldSize < newSize ? oldSize : newSize);
                vPortFree(ptr);
            }
            result = newPtr;
        }
    }
    return (result);
}

/**
 * @brief Log that a FreeRTOS object could not be created, and stop.
 *
 * @param what Description of the object that could not be created.
 */
void Utils::CreationFailed(const char * const what)
{
    ERROR_MESSAGE("Cannot create %s.", what);
    ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    //
    //  ESP_ERROR_CHECK does not return for an error, but the compiler cannot tell.
    //
    abort();
}

/**
 * @brief Check that xTaskCreate or xTaskCreatePinnedToCore succeeded, stopping with a diagnostic if not.
 *
 * @param result Value returned by the creation call.
 * @param what Description of the task, for the diagnostic.
 */
void Utils::RequireTaskCreated(const BaseType_t result, const char * const what)
{
    if (result != pdPASS)
    {
        CreationFailed(what);
    }
}
