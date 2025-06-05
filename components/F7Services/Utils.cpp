/**
 * @file Utils.cpp
 * @author Wilderness Labs
 * @brief Utility functions for the ESP32 application.
 * @version 0.1
 * @date 2025-06-05
 *
 * @copyright Copyright (c) 2025
 */

#include "sdkconfig.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <string>

#include "Utils.hpp"

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
