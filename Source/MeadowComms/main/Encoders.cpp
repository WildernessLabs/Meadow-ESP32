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
