/**
 * @file PayloadReader.hpp
 * @author Mark Stevens
 * @brief Reads the fields of a message payload without reading past its end.
 * @date 2026-10-04
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#pragma once

#include <stdint.h>

/**
 *  @brief Reads the fields of a payload one at a time, never reading past the end of it.
 *
 *  Every length a payload declares comes from the STM32 and is not trusted.  The reader is told
 *  how many bytes the payload really holds, and a read of more than is left does not happen:
 *  it marks the reader as failed instead.  Once failed, every later read returns 0 or nullptr,
 *  so a decoder reads all its fields and checks Failed() once at the end (issue #226).
 */
class PayloadReader
{
private:
    /**
     *  @brief Next byte to be read.
     */
    const uint8_t *_buffer;

    /**
     *  @brief Number of bytes left to read.
     */
    uint32_t _remaining;

    /**
     *  @brief Has a read been refused?
     */
    bool _failed = false;

    /**
     *  @brief Was the refusal a failure to allocate memory, rather than a payload that is too short?
     */
    bool _outOfMemory = false;

    /**
     *  @brief Claim the next bytes of the payload.
     *
     *  @param count Number of bytes wanted.
     *
     *  @return The first of them, or nullptr (and the reader is failed) if fewer are left.
     */
    const uint8_t *Take(const uint32_t count);

public:
    /**
     *  @brief Start reading a payload.
     *
     *  @param buffer Start of the payload, nullptr if there is none.
     *  @param length Number of bytes in the payload.
     */
    PayloadReader(const uint8_t *buffer, const uint32_t length);

    /**
     *  @brief Read one byte.
     *
     *  @return The byte, 0 if the reader has failed.
     */
    uint8_t ReadByte();

    /**
     *  @brief Read a 16-bit unsigned integer, least significant byte first.
     *
     *  @return The value, 0 if the reader has failed.
     */
    uint16_t ReadUInt16();

    /**
     *  @brief Read a 32-bit unsigned integer, least significant byte first.
     *
     *  @return The value, 0 if the reader has failed.
     */
    uint32_t ReadUInt32();

    /**
     *  @brief Read a 32-bit signed integer, least significant byte first.
     *
     *  @return The value, 0 if the reader has failed.
     */
    int32_t ReadInt32();

    /**
     *  @brief Copy a fixed number of bytes into a buffer the caller owns.
     *
     *  @param destination Where to put the bytes.
     *  @param count Number of bytes to copy.
     */
    void ReadBytes(uint8_t * const destination, const uint32_t count);

    /**
     *  @brief Copy a block whose length was read from the payload into new memory.
     *
     *  The length is checked against what is left before anything is allocated, so a payload
     *  that declares more than it carries is refused rather than allocated and copied.
     *
     *  @param count Number of bytes in the block.
     *
     *  @return The block, which the caller releases, or nullptr if count is 0, the payload is too
     *          short or there is no memory (the last two fail the reader).
     */
    uint8_t *ReadBlock(const uint32_t count);

    /**
     *  @brief Copy a null terminated string into new memory.
     *
     *  @return The string, which the caller releases, or nullptr if it is empty, the payload ends
     *          before the terminator or there is no memory (the last two fail the reader).
     */
    char *ReadString();

    /**
     *  @brief Has a read been refused?
     *
     *  @return true if the payload was too short for what was read, or memory ran out.
     */
    bool Failed() const;

    /**
     *  @brief Was a read refused because memory ran out, rather than because the payload was too short?
     *
     *  @return true if the failure was running out of memory.
     */
    bool OutOfMemory() const;
};
