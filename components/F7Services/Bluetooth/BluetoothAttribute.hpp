/**
 * @file BluetoothAttribute.hpp
 * @author Chris Tacke
 * @brief Hold the definitions for the BluetoothAttribute class, CharacteristicProperty enum, and CharacteristicPermission enum.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include <cstdint>

/**
 * @brief Property type of this characteristic
 */
enum class CharacteristicProperty : unsigned char { Broadcast = 1U << 0U, Read = 1U << 1U, WriteNoResponse = 1U << 2U, Write = 1U << 3U, Notify = 1U << 4U, Indicate = 1U << 5U, SignedWrite = 1U << 6U, ExtendedProp = 1U << 7U };

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
    return static_cast<CharacteristicProperty>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
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
    return static_cast<CharacteristicProperty>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * @brief Permissions for a characteristic.
 */
enum class CharacteristicPermission : unsigned short {
    Read = 1U << 0U,
    ReadEncrypted = 1U << 1U,
    ReadEncMITM = 1U << 2U,
    // WHERE IS 1 << 3?
    Write = 1U << 4U,
    WriteEncrypted = 1U << 5U,
    WriteEncMITM = 1U << 6U,
    WriteSigned = 1U << 7U,
    WriteSignedMITM = 1U << 8U
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
    return static_cast<CharacteristicPermission>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
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
     *
     *  @note Defaults to no permission bits set until a constructor assigns the real
     *        value, rather than leaving it indeterminate.
     */
    CharacteristicPermission Permission = static_cast<CharacteristicPermission>(0);

    /**
     * @brief Handle for this attribute
     *
     *  @note 0x0000 is reserved in the Bluetooth GATT specification and never assigned
     *        to a real attribute, so it doubles as an unambiguous "not yet assigned"
     *        sentinel until BluetoothHandleManager gives this attribute a real handle.
     */
    uint16_t Handle = 0x0000;
};