/**
 * @file PayloadReader.cpp
 * @author Mark Stevens
 * @brief Reads the fields of a message payload without reading past its end.
 * @date 2026-10-04
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "freertos/FreeRTOS.h"

#include <string.h>

#include "Encoders.hpp"
#include "PayloadReader.hpp"

/**
 *  @brief Start reading a payload.
 *
 *  @param buffer
 *      Start of the payload, nullptr if there is none, which holds nothing to read.
 *
 *  @param length
 *      Number of bytes in the payload.
 */
PayloadReader::PayloadReader(const uint8_t *buffer, const uint32_t length) : _buffer(buffer), _remaining((buffer == nullptr) ? 0 : length)
{
}

/**
 *  @brief Claim the next bytes of the payload.
 *
 *  @param count
 *      Number of bytes wanted.
 *
 *  @returns
 *      The first of them, or nullptr (and the reader is failed) if fewer are left.
 */
const uint8_t *PayloadReader::Take(const uint32_t count)
{
    if (_failed || (count > _remaining))
    {
        _failed = true;
        return (nullptr);
    }

    const uint8_t *start = _buffer;
    _buffer += count;
    _remaining -= count;
    return (start);
}

/**
 *  @brief Read one byte.
 *
 *  @returns
 *      The byte, 0 if the reader has failed.
 */
uint8_t PayloadReader::ReadByte()
{
    const uint8_t *field = Take(1);
    return ((field == nullptr) ? 0 : *field);
}

/**
 *  @brief Read a 16-bit unsigned integer, least significant byte first.
 *
 *  @returns
 *      The value, 0 if the reader has failed.
 */
uint16_t PayloadReader::ReadUInt16()
{
    const uint8_t *field = Take(2);
    return ((field == nullptr) ? 0 : Encoders::ExtractUInt16(field));
}

/**
 *  @brief Read a 32-bit unsigned integer, least significant byte first.
 *
 *  @returns
 *      The value, 0 if the reader has failed.
 */
uint32_t PayloadReader::ReadUInt32()
{
    const uint8_t *field = Take(4);
    return ((field == nullptr) ? 0 : Encoders::ExtractUInt32(field));
}

/**
 *  @brief Read a 32-bit signed integer, least significant byte first.
 *
 *  @returns
 *      The value, 0 if the reader has failed.
 */
int32_t PayloadReader::ReadInt32()
{
    return (static_cast<int32_t>(ReadUInt32()));
}

/**
 *  @brief Copy a fixed number of bytes into a buffer the caller owns.
 *
 *  @param destination
 *      Where to put the bytes.
 *
 *  @param count
 *      Number of bytes to copy.
 */
void PayloadReader::ReadBytes(uint8_t * const destination, const uint32_t count)
{
    const uint8_t *field = Take(count);
    if (field != nullptr)
    {
        memcpy(destination, field, count);
    }
}

/**
 *  @brief Copy a block whose length was read from the payload into new memory.
 *
 *  The length is checked against what is left before anything is allocated, so a payload that
 *  declares more than it carries is refused rather than allocated and copied.
 *
 *  @param count
 *      Number of bytes in the block.
 *
 *  @returns
 *      The block, which the caller releases, or nullptr if count is 0, the payload is too short
 *      or there is no memory (the last two fail the reader).
 */
uint8_t *PayloadReader::ReadBlock(const uint32_t count)
{
    if (_failed || (count == 0))
    {
        return (nullptr);
    }

    const uint8_t *field = Take(count);
    if (field == nullptr)
    {
        return (nullptr);
    }

    uint8_t *block = static_cast<uint8_t *>(pvPortMalloc(count));
    if (block == nullptr)
    {
        _failed = true;
        _outOfMemory = true;
        return (nullptr);
    }
    memcpy(block, field, count);
    return (block);
}

/**
 *  @brief Copy a null terminated string into new memory.
 *
 *  @returns
 *      The string, which the caller releases, or nullptr if it is empty, the payload ends before
 *      the terminator or there is no memory (the last two fail the reader).
 */
char *PayloadReader::ReadString()
{
    if (_failed)
    {
        return (nullptr);
    }

    const void *terminator = memchr(_buffer, 0, _remaining);
    if (terminator == NULL)
    {
        _failed = true;
        return (nullptr);
    }

    const uint32_t length = static_cast<uint32_t>(static_cast<const uint8_t *>(terminator) - _buffer);
    const uint8_t *field = Take(length + 1);
    if (length == 0)
    {
        return (nullptr);
    }

    char *result = static_cast<char *>(pvPortMalloc(length + 1));
    if (result == nullptr)
    {
        _failed = true;
        _outOfMemory = true;
        return (nullptr);
    }
    memcpy(result, field, length + 1);
    return (result);
}

/**
 *  @brief Has a read been refused?
 *
 *  @returns
 *      true if the payload was too short for what was read, or memory ran out.
 */
bool PayloadReader::Failed() const
{
    return (_failed);
}

/**
 *  @brief Was a read refused because memory ran out, rather than because the payload was too short?
 *
 *  @returns
 *      true if the failure was running out of memory.
 */
bool PayloadReader::OutOfMemory() const
{
    return (_outOfMemory);
}
