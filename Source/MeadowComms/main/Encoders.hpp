/*
 *  Encoders.hpp
 *
 *  Provide methods to allow the extraction and encoding of data in buffer.
 */

#ifndef MAIN_ENCODERS_HPP_
#define MAIN_ENCODERS_HPP_

#include "freertos/FreeRTOS.h"

/*
 *      Class holding the encoding methods.
 *
 *      Functionality includes:
 *          - Encoding 16 / 32 bit values into buffers.
 *          - Extracting 16 / 32 bit values from buffer.
 *          - CRC calculations.
 */
class Encoders
{
public:
    /*
     *      Seed for the 32-bit CRC calculation.
     */
    static const uint32_t CRC32Seed = 0xffffffff;

    static uint16_t ExtractUInt16(uint8_t *);
    static void EncodeUInt16(uint16_t, uint8_t *);
    static uint32_t ExtractUInt32(uint8_t *);
    static void EncodeUInt32(uint32_t, uint8_t *);
    static uint8_t CRC8(const uint8_t *, uint16_t);
    static uint32_t CRC32(const uint8_t *, uint16_t);
    static uint32_t ProgressiveCRC32(uint32_t, uint8_t);
};


#endif /* MAIN_ENCODERS_HPP_ */
