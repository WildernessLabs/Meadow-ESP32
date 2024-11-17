/**
 * @file BluetoothAttribute.hpp
 * @author Chris Tacke
 *
 * @brief Hold the definitions for the following:
 *  - BluetoothAttribute class
 *  - CharacteristicProperty enum
 *  - CharacteristicPermission enum
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#ifndef _BLUETOOTH_ATTRIBUTE_HPP_
#define _BLUETOOTH_ATTRIBUTE_HPP_

#include "sdkconfig.h"

#include "bt_compat.hpp"

#define LOG_COMPONENT_NAME "BTTask"

// const char *BluetoothAttribute::COMPONENT_NAME = "BTTask";

/**
 * @brief Property type of this characteristic
 */
enum class CharacteristicProperty : unsigned char { Broadcast = 1 << 0, Read = 1 << 1, WriteNoResponse = 1 << 2, Write = 1 << 3, Notify = 1 << 4, Indicate = 1 << 5, SignedWrite = 1 << 6, ExtendedProp = 1 << 7 };

/**
 * @brief Overload the bitwise OR operator to allow for combining characteristic properties.
 *
 * @param a First permission to combine.
 *
 * @param b Second permission to combine.
 *
 * @return The combined permission.
 */
inline CharacteristicProperty operator|(CharacteristicProperty a, CharacteristicProperty b)
{
    return static_cast<CharacteristicProperty>(static_cast<int>(a) | static_cast<int>(b));
}

/**
 * @brief Permissions for a characteristic.
 */
enum class CharacteristicPermission : unsigned short {
    Read = 1 << 0,
    ReadEncrypted = 1 << 1,
    ReadEncMITM = 1 << 2,
    // WHERE IS 1 << 3?
    Write = 1 << 4,
    WriteEncrypted = 1 << 5,
    WriteEncMITM = 1 << 6,
    WriteSigned = 1 << 7,
    WriteSignedMITM = 1 << 8
};

/**
 * @brief Overload the bitwise OR operator to allow for combining characteristic permissions.
 *
 * @param a First permission to combine.
 *
 * @param b Second permission to combine.
 *
 * @return The combined permission.
 */
inline CharacteristicPermission operator|(CharacteristicPermission a, CharacteristicPermission b)
{
    return static_cast<CharacteristicPermission>(static_cast<int>(a) | static_cast<int>(b));
}

/**
 * @brief Overload the bitwise AND operator to allow for combining characteristic permissions.
 */
class BluetoothAttribute
{
public:
    /**
     * @brief Permissions for this attribute
     */
    CharacteristicPermission Permission;

    /**
     * @brief Handle for this attribute
     */
    uint16_t Handle;
};

#endif