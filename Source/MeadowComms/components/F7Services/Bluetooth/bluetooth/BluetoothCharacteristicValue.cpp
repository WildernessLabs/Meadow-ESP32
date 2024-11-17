/**
 * @file BluetoothCharacteristicValue.cpp
 * @author Chris Tacke
 * 
 * @brief Implementation of methods in the BluetoothCharacteristicValue class.
 * 
 * @copyright Copyright (c) 2019 - 2024
 */
#include "BluetoothCharacteristicValue.hpp"

/**
 *  @brief Create a new characteristic value.
 *
 *  @param parent Parent of this value.
 *
 *  @param uuid 16-bit UUID for this characteristic value.
 *
 *  @param permission Permissions for this characteristic value.
 *
 *  @param maxValueLength Maximum length of the value that can be stored in the characteristic value.
 */
BluetoothCharacteristicValue::BluetoothCharacteristicValue(BluetoothCharacteristic *parent, Uuid16 uuid, CharacteristicPermission permission, int maxValueLength)
{
    _parent = parent;
    uuid16 = uuid;
    Permission = permission;
    _value = new uint8_t[maxValueLength];
    MaxLength = maxValueLength;
}

/**
 *  @brief Create a new characteristic value.
 *
 *  @param parent Parent of this value.
 *
 *  @param uuid 128-bit UUID for this characteristic value.
 *
 *  @param permission Permissions for this characteristic value.
 *
 *  @param maxValueLength Maximum length of the value that can be stored in the characteristic value.
 */
BluetoothCharacteristicValue::BluetoothCharacteristicValue(BluetoothCharacteristic *parent, Uuid128 uuid, CharacteristicPermission permission, int maxValueLength)
{
    _parent = parent;
    uuid128 = uuid;
    Permission = permission;
    _value = new uint8_t[maxValueLength];
    MaxLength = maxValueLength;
    _is128 = true;
}

/**
 *  @brief Delete the characteristic value releasing any allocated resources.
 */
BluetoothCharacteristicValue::~BluetoothCharacteristicValue()
{
    if (_value)
    {
        //
        //  Suppressing the cppcheck warning here because the value is allocated in the constructor.
        //
        // cppcheck-suppress mismatchAllocDealloc
        delete _value;
        _value = NULL;
    }
}


/**
 *  @brief Write a new value to the characteristic.
 *
 *  @param source Value to be written.
 *
 *  @param length Length of the value to be written.
 *
 *  @return True if the value was written successfully, false otherwise.
 */
bool BluetoothCharacteristicValue::WriteValue(uint8_t *source, int length)
{
    if (length > MaxLength)
    {
        return false;
    }
    if (length < 0)
    {
        return false;
    }

    if (length > 0)
    {
        memcpy(_value, source, length);

        if (_parent->OnValueChanged)
        {
            _parent->OnValueChanged(this->_parent);
        }
    }

    return true;
}

/**
 *  @brief Read a value from the characteristic.
 *
 *  @param dest Destination for the value to be read.
 *
 *  @param length Length of the destination buffer.
 *
 *  @return True if the value was read successfully, false otherwise.
 */
bool BluetoothCharacteristicValue::ReadValue(uint8_t *dest, int length)
{
    if (length > CurrentLength)
    {
        return false;
    }
    if (length < 0)
    {
        return false;
    }

    if (length > 0)
    {
        memcpy(dest, _value, length);
    }

    return true;
}
