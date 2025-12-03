/**
 * @file BluetoothCharacteristicDeclaration.hpp
 * @author Mark Stevens
 * @brief Implementation for the BluetoothCharacteristicDeclaration class.
 * @date 2025-12-02
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#include "BluetoothCharacteristicDeclaration.hpp"

/**
 * @brief Construct a new Bluetooth Characteristic Declaration object
 *
 * @param value_uuid 16-bit UUID for the characteristic.
 * @param permission Permissions for the characteristic.
 * @param properties Properties of the characteristic.
 */
BluetoothCharacteristicDeclaration::BluetoothCharacteristicDeclaration(const Uuid16 value_uuid, const CharacteristicPermission permission, const CharacteristicProperty properties)
{
    Uuid = Uuid16::CharacteristicDeclaration;
    Permission = permission;
    Properties = properties;
    ValueUuid16 = value_uuid;
}

/**
 * @brief Construct a new Bluetooth Characteristic Declaration object
 *
 * @param value_uuid 128-bit UUID for the characteristic.
 * @param permission Permissions for the characteristic.
 * @param properties Properties of the characteristic.
 */
BluetoothCharacteristicDeclaration::BluetoothCharacteristicDeclaration(const Uuid128 value_uuid, const CharacteristicPermission permission, const CharacteristicProperty properties)
{
    Uuid = Uuid16::CharacteristicDeclaration;
    Permission = permission;
    Properties = properties;
    ValueUuid128 = value_uuid;
    _is128 = true;
}

/**
 * @brief Destroy the BluetoothCharacteristicDeclaration object and release any allocated resources.
 */
BluetoothCharacteristicDeclaration::~BluetoothCharacteristicDeclaration()
{
    // if (_is128)
    // {
    //     delete ValueUuid128;
    // }
    // else
    // {
    //     delete ValueUuid16;
    // }
}

/**
 * @brief Is the UUID 16 or 128 bit?
 *
 * @return true The UUID is a 128-bit UUID.
 * @return false The UUID is a 16-bit UUID.
 */
bool BluetoothCharacteristicDeclaration::Is128()
{
    return _is128;
}
