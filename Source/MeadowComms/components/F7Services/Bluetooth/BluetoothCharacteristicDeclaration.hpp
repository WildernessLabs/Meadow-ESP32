/**
 * @file BluetoothCharacteristicDeclaration.hpp
 * @author Chris Tacke
 *
 * @brief Class declaration for the BluetoothCharacteristicDeclaration class.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#ifndef _BLUETOOTH_CHARACTERISTIC_DECLARATION_HPP_
#define _BLUETOOTH_CHARACTERISTIC_DECLARATION_HPP_

#include <string>
#include <vector>

#include "BluetoothAttribute.hpp"
#include "BluetoothCharacteristic.hpp"

using namespace std;

/**
 * @brief Class holding a Bluetooth characteristic declaration.
 */
class BluetoothCharacteristicDeclaration : public BluetoothAttribute
{
private:
    /**
     * @brief Is the UUID 16 or 128 bit?
     */
    bool _is128 = false;

public:
    /**
     * @brief Construct a new Bluetooth Characteristic Declaration object
     *
     * @param value_uuid 16-bit UUID for the characteristic.
     * @param permission Permissions for the characteristic.
     * @param properties Properties of the characteristic.
     */
    BluetoothCharacteristicDeclaration(Uuid16 value_uuid, CharacteristicPermission permission, CharacteristicProperty properties)
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
    BluetoothCharacteristicDeclaration(Uuid128 value_uuid, CharacteristicPermission permission, CharacteristicProperty properties)
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
    ~BluetoothCharacteristicDeclaration()
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
    bool Is128()
    {
        return _is128;
    }

    /**
     * @brief 16-bit UUID for this characteristic.
     */
    Uuid16 ValueUuid16;

    /**
     * @brief 128-bit UUID for this characteristic.
     */
    Uuid128 ValueUuid128;
};

#endif