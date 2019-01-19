/*
 *  Encoders.cpp
 *
 *  Global methods providing encoding functionality.
 */

#include "Encoders.hpp"

/*
 *      Take the first two bytes from the buffer and encode them as a 16 bit integer.
 *
 *      Note that the data should be encoded as LSB first.
 *
 *      @param buffer
 *          Pointer to the buffer holding the data that should be used to created the
 *          16-bit unsigned integer.
 *
 *      @return
 *          16-bit unsigned integer value from the first two byte in the buffer.
 */
uint16_t Encoders::ExtractUInt16(uint8_t *buffer)
{
    uint16_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    return (result);
}

/*
 *      Encode a 16-bit integer as two bytes in the buffer.
 *
 *      Note that the data will be encoded LSB first.
 *
 *      @param value
 *          16-bit value to encode in the first two bytes of the buffer.
 *
 *      @param buffer
 *          Pointer to the buffer where the two bytes will be inserted
 *          from the 16-bit unsigned integer (LSB first).
 */
void Encoders::EncodeUInt16(uint16_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
}

/*
 *      Take the first four bytes from the buffer and encode them as a 32 bit integer.
 *
 *      Note that the data should be encoded as LSB first.
 *
 *      @param buffer
 *          Pointer to the buffer where the next four bytes should be extracted
 *          and a 32-bit unsigned integer created (LSB first).
 *
 *      @return
 *          32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 */
uint32_t Encoders::ExtractUInt32(uint8_t *buffer)
{
    uint32_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    result |= (buffer[2] << 16);
    result |= (buffer[3] << 24);
    return (result);
}

/*
 *      Encode a 32-bit integer as four bytes in the buffer.
 *
 *      Note that the data will be encoded LSB first.
 *
 *      @param value
 *          Unsigned 32-bit integer value to write into the buffer.
 *
 *      @param buffer
 *          Pointer to the buffer where 4 bytes will be replaced with the four
 *          bytes representing unsigned integer (LSB first).
 */
void Encoders::EncodeUInt32(uint32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/*
 *      Calculate the 8-bit CRC value for the specified data buffer.
 *
 *      This algorithm is loosely based upon the Dallas 1-Wire algorithm.
 *
 *      @param data
 *          Pointer to the buffer containing the data for which the CRC should be calculated.
 *
 *      @param len
 *          Number of bytes in the data buffer.
 *
 *      @return
 *          8-bit checksum of the bytes in the buffer.
 */
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

/*
 *      Calculate the 32-bit CRC of the buffer of data.
 *
 *      @param data
 *          Pointer to the buffer containing the data for which the CRC should be calculated.
 *
 *      @param len
 *          Number of bytes in the data buffer.
 *
 *      @return
 *          32-bit checksum of the bytes in the buffer.
 */
uint32_t Encoders::CRC32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = CRC32_SEED;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        crc = ProgressiveCRC32(crc, data[byteCounter]);
    }
    return (crc);
}

/*
 *      Calculate the CRC32 value change for the specified byte.
 *
 *      @param currentChecksum
 *          Current value of the checksum.
 *
 *      @param byte
 *          Next byte to use in the checksum calculation.
 *
 *      @return
 *          Next checksum value.
 */
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
