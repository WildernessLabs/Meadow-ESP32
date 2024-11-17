/**
 * @file Uuid.cpp
 * @author Chris Tacke.
 * 
 * @brief Implementation of the methods operating on 16 and 128 bit UUIDs.
 * 
 * @copyright Copyright (c) 2019 - 2024
 */
#include "Uuid.hpp"

/**
 * Overview
 * 
 * Taken from Nordic Semiconductor's BLE characteristics for Beginners:
 * 
 * https://devzone.nordicsemi.com/guides/short-range-guides/b/bluetooth-low-energy/posts/ble-characteristics-a-beginners-tutorial
 * 
 * GATT groups attributes together in a specific order.  This order has the Service Declaration at the top
 * of the group.  This is then followed by the Characteristic Declaration.  These two properties have specific UUIDs
 * and the two items are defined in the variables
 *  - PrimaryService
 *  - CharacteristicDeclaration
 */

/**
 * @brief Primary service UUID.
 * 
 * @note This is the service at the top of each group.  It has the standard type of 0x2800.
 */
Uuid16 Uuid16::PrimaryService = Uuid16((uint16_t) 0x2800);

/**
 * @brief Characteristic declaration UUID for the Primary Service.
 */
Uuid16 Uuid16::CharacteristicDeclaration = Uuid16((uint16_t) 0x2803);

/**
 * @brief Construct a new Uuid16 object (16-bit UUID).
 */
Uuid16::Uuid16()
{
}

/**
 * @brief Construct a new Uuid16 object with the specified value.
 * 
 * @param value Initial value for the UUID.
 */
Uuid16::Uuid16(uint16_t value)
{
    memcpy((void *) data, &value, 2);
}

/**
 * @brief Construct a new Uuid16 object from a byte array.
 * 
 * @param other Pointer to the byte array containing the UUID to be copied.
 */
Uuid16::Uuid16(uint8_t *other)
{
    memcpy((void *) data, other, 2);
}

/**
 * @brief Convert a hexadecimal character to a number.
 * 
 * @param ch Character to be converted.
 * @return unsigned char 
 */
unsigned char HexDigitToChar(char ch)
{
    // 0-9
    if (ch > 47 && ch < 58)
        return ch - 48;

    // a-f
    if (ch > 96 && ch < 103)
        return ch - 87;

    // A-F
    if (ch > 64 && ch < 71)
        return ch - 55;

    return 0;
}

/**
 * @brief Check if the character is a valid hexadecimal character.
 * 
 * @param ch Character to be checked.
 * @return true If the character is a valid hexadecimal character.
 * @return false If the character is not a valid hexadecimal character.
 */
bool IsValidHexChar(char ch)
{
    // 0-9
    if (ch > 47 && ch < 58)
        return true;

    // a-f
    if (ch > 96 && ch < 103)
        return true;

    // A-F
    if (ch > 64 && ch < 71)
        return true;

    return false;
}

// converts the two hexadecimal characters to an unsigned char (a byte)
/**
 * @brief Convert a pair of characters to a byte.
 * 
 * @param a High 4-bits of the byte.
 * @param b Lower 4-bits of the byte.
 * @return unsigned char Byte value represented by the two characters.
 */
unsigned char hexPairToChar(char a, char b)
{
    return HexDigitToChar(a) * 16 + HexDigitToChar(b);
}

/**
 * @brief Construct a new Uuid128 object with the default values.
 */
Uuid128::Uuid128()
{
}

/**
 * @brief Construct a new Uuid128 from an array of bytes.
 * 
 * @param other 16 bytes of data holding the UUID to be copied.
 */
Uuid128::Uuid128(uint8_t *other)
{
    memcpy((void *) data, other, 16);
}

/**
 * @brief Convert a UUID128 (GUID) into a string.
 * 
 * @return std::string String representation of the UUID.
 */
std::string Uuid128::ToString() const
{
    char one[10], two[6], three[6], four[6], five[14];

    snprintf(one, 10, "%02x%02x%02x%02x", data[15], data[14], data[13], data[12]);
    snprintf(two, 6, "%02x%02x", data[11], data[10]);
    snprintf(three, 6, "%02x%02x", data[9], data[8]);
    snprintf(four, 6, "%02x%02x", data[7], data[6]);
    snprintf(five, 14, "%02x%02x%02x%02x%02x%02x", data[5], data[4], data[3], data[2], data[1], data[0]);
    const std::string sep("-");
    std::string out(one);

    out += sep + two;
    out += sep + three;
    out += sep + four;
    out += sep + five;

    return out;
}

// create a guid from string
/**
 * @brief Construct a new Uuid128 object from a string.
 * 
 * @param fromString String containing the UUID (GUID).
 */
Uuid128::Uuid128(const char *fromString)
{
    char charOne = '\0';
    char charTwo = '\0';
    bool lookingForFirstChar = true;
    unsigned nextByte = 0;

    auto len = strlen(fromString);

    for (int i = len - 1; i >= 0; i--)
    {
        char ch = fromString[i];

        if (ch == '-')
        {
            continue;
        }

        if (nextByte >= 16 || !IsValidHexChar(ch))
        {
            // Invalid string so bail
            memset(data, 0, 16);
            return;
        }

        if (lookingForFirstChar)
        {
            charOne = ch;
            lookingForFirstChar = false;
        }
        else
        {
            charTwo = ch;
            auto byte = hexPairToChar(charTwo, charOne);
            data[nextByte++] = byte;
            lookingForFirstChar = true;
        }
    }

    // if there were fewer than 16 bytes in the string then guid is bad
    if (nextByte < 16)
    {
        memset(data, 0, 16);
        return;
    }
}