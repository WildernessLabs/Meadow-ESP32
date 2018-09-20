#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "stdlib.h"

/// <summary>
///     Take the first two bytes from the buffer and encode them as a 16
///     bit integer.  Note that the data should be encoded as LSB first.
/// </summary>
uint16_t ExtractUInt16(uint8_t *buffer)
{
    uint16_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    return (result);
}

/// <summary>
///     Encode a 16-bit integer as four bytes in the buffer.  The data is
///     encoded LSB first.
/// </summary>
void EncodeUInt16(uint16_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
}

/// <summary>
///     Take the first four bytes from the buffer and encode them as a 32
///     bit integer.  Note that the data should be encoded as LSB first.
/// </summary>
uint32_t ExtractUInt32(uint8_t *buffer)
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
void EncodeUInt32(uint32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}