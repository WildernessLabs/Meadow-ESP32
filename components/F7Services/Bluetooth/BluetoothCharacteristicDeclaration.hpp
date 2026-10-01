/**
 * @file BluetoothCharacteristicDeclaration.hpp
 * @author Chris Tacke
 * @brief Class definition for the BluetoothCharacteristicDeclaration class.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include "BluetoothAttribute.hpp"
#include "Uuid.hpp"

/**
 * @brief Class holding a Bluetooth characteristic declaration.
 */
class BluetoothCharacteristicDeclaration : public BluetoothAttribute
{
public:
    /**
     * @brief Construct a new Bluetooth Characteristic Declaration object
     *
     * @param value_uuid 16-bit UUID for the characteristic.
     * @param permission Permissions for the characteristic.
     * @param properties Properties of the characteristic.
     */
    BluetoothCharacteristicDeclaration(const Uuid16 value_uuid, const CharacteristicPermission permission, const CharacteristicProperty properties);

    /**
     * @brief Construct a new Bluetooth Characteristic Declaration object
     *
     * @param value_uuid 128-bit UUID for the characteristic.
     * @param permission Permissions for the characteristic.
     * @param properties Properties of the characteristic.
     */
    BluetoothCharacteristicDeclaration(const Uuid128 value_uuid, const CharacteristicPermission permission, const CharacteristicProperty properties);

    /**
     * @brief Destroy the BluetoothCharacteristicDeclaration object and release any allocated resources.
     */
    ~BluetoothCharacteristicDeclaration() override;

    /**
     * @brief 16-bit UUID for this characteristic declaration.
     */
    Uuid16 Uuid;

    /**
     * @brief Permitted operations on this characteristic.
     */
    CharacteristicProperty Properties;

    /**
     * @brief Handle of the attribute containing the characteristic value
     */
    uint16_t ValueHandle;

    /**
     * @brief Is the UUID 16 or 128 bit?
     *
     * @return true The UUID is a 128-bit UUID.
     * @return false The UUID is a 16-bit UUID.
     */
    bool Is128();

    /**
     * @brief 16-bit UUID for this characteristic.
     */
    Uuid16 ValueUuid16;

    /**
     * @brief 128-bit UUID for this characteristic.
     */
    Uuid128 ValueUuid128;

private:
    /**
     * @brief Is the UUID 16 or 128 bit?
     */
    bool _is128 = false;
};