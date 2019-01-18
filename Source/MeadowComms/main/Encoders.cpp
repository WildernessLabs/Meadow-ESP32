/*
 * Encoders.cpp
 *
 *  Created on: 18 Jan 2019
 *      Author: markstevens
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include <string.h>
#include <stdlib.h>


#include "Encoders.hpp"


/// <summary>
///     Take the first two bytes from the buffer and encode them as a 16
///     bit integer.  Note that the data should be encoded as LSB first.
/// </summary>
uint16_t Encoders::ExtractUInt16(uint8_t *buffer)
{
    uint16_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    return (result);
}

/// <summary>
///     Encode a 16-bit integer as two bytes in the buffer.  The data is
///     encoded LSB first.
/// </summary>
void Encoders::EncodeUInt16(uint16_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
}

/// <summary>
///     Take the first four bytes from the buffer and encode them as a 32
///     bit integer.  Note that the data should be encoded as LSB first.
/// </summary>
uint32_t Encoders::ExtractUInt32(uint8_t *buffer)
{
    uint32_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    result |= (buffer[2] << 16);
    result |= (buffer[3] << 24);
    return (result);
}

/// <summary>
///     Encode a 32-bit integer as four bytes in the buffer.  The data is
///     encoded LSB first.
/// </summary>
void Encoders::EncodeUInt32(uint32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/// <summary>
///     Calculate the CRC value for the specified data buffer.
/// </summary>
/// <remarks>
///     This algorithm is loosely based upon the Dallas 1-Wire algorithm.
/// </remarks>
uint8_t Encoders::CRC8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        uint8_t byte = data[byteCounter];
        for (uint8_t bitCounter = 8; bitCounter; bitCounter--)
        {
            uint8_t sum = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (sum)
            {
                crc ^= 0x8C;
            }
            byte >>= 1;
        }
    }
    return(crc);
}

/// <summary>
///     Calculate the 32-bit CRC of a buffer of data.
/// </summary>
uint32_t Encoders::CRC32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = CRC32_SEED;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        crc = ProgressiveCRC32(crc, data[byteCounter]);
    }
    return (crc);
}

/// <summary>
///     Calculate the CRC32 value change for the specified byte.
/// </summary>
uint32_t Encoders::ProgressiveCRC32(uint32_t currentChecksum, uint8_t byte)
{
    uint32_t crc = currentChecksum;
    crc ^= byte;
    for (uint32_t index = 0; index < 8; index++)
    {
        uint32_t mask = (uint32_t) -(crc & 0x01);
        crc = (crc >> 1) ^ (0xedb88320 & mask);
    }
    return (crc);
}
