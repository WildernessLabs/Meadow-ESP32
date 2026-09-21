/**
 * @file BluetoothAttribute.hpp
 * @author Chris Tacke
 * @brief Hold the definitions for the BluetoothAttribute class, CharacteristicProperty enum, and CharacteristicPermission enum.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include <stdint.h>

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
 * @brief Overload the bitwise AND operator to allow for extracting a specific characteristic permissions.
 *
 * @param a First permission to extract.
 *
 * @param b Second permission to extract.
 *
 * @return Specific permission if it is set.
 */
inline CharacteristicProperty operator&(CharacteristicProperty a, CharacteristicProperty b)
{
    return static_cast<CharacteristicProperty>(static_cast<int>(a) & static_cast<int>(b));
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
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Virtual destructor.  BluetoothCharacteristicDeclaration, BluetoothCharacteristicValue
     *  and BluetoothDescriptor are all deleted dynamically during normal service teardown; without
     *  this, deleting one of them through a BluetoothAttribute pointer would be undefined behaviour.
     */
    virtual ~BluetoothAttribute() = default;

    /**
     * @brief Permissions for this attribute
     */
    CharacteristicPermission Permission;

    /**
     * @brief Handle for this attribute
     */
    uint16_t Handle;
};