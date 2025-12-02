/**
 * @file BluetoothCharacteristicValue.cpp
 * @author Chris Tacke
 * @brief Implementation of BluetoothCharacteristicValue class.
 * @date 2021-02-23
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#include "BluetoothCharacteristicValue.hpp"
#include "BluetoothComponentName.hpp"

#include "Logging.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

const char *BluetoothCharacteristicValue::COMPONENT_NAME = BLUETOOTH_COMPONENT_NAME;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

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
BluetoothCharacteristicValue::BluetoothCharacteristicValue(BluetoothCharacteristic * const parent, const Uuid16 uuid, const CharacteristicPermission permission, const int maxValueLength)
{
    TRACE_MESSAGE("BluetoothCharacteristicValue(UUID16): Enter");

    _parent = parent;
    uuid16 = uuid;
    Permission = permission;
    _value = new uint8_t[maxValueLength];
    MaxLength = maxValueLength;

    TRACE_MESSAGE("BluetoothCharacteristicValue(UUID16): Exit");
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
BluetoothCharacteristicValue::BluetoothCharacteristicValue(BluetoothCharacteristic * const parent, const Uuid128 uuid, const CharacteristicPermission permission, const int maxValueLength)
{
    TRACE_MESSAGE("BluetoothCharacteristicValue(UUID128): Enter");

    _parent = parent;
    uuid128 = uuid;
    Permission = permission;
    _value = new uint8_t[maxValueLength];
    MaxLength = maxValueLength;
    _is128 = true;

    TRACE_MESSAGE("BluetoothCharacteristicValue(UUID128): Exit");
}

/**
 *  @brief Delete the characteristic value releasing any allocated resources.
 */
BluetoothCharacteristicValue::~BluetoothCharacteristicValue()
{
    TRACE_MESSAGE("~BluetoothCharacteristicValue: Enter");

    if (_value)
    {
        // if (_is128)
        // {
        //     delete uuid128;
        // }
        // else
        // {
        //     delete uuid16;
        // }
        //
        //  Suppressing the cppcheck warning here because the value is allocated in the constructor.
        //
        // cppcheck-suppress mismatchAllocDealloc
        delete[] _value;
        _value = NULL;
    }

    TRACE_MESSAGE("~BluetoothCharacteristicValue: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Write a new value to the characteristic.
 *
 *  @param source Value to be written.
 *
 *  @param length Length of the value to be written.
 *
 *  @return True if the value was written successfully, false otherwise.
 */
bool BluetoothCharacteristicValue::WriteValue(const uint8_t * const source, const int length)
{
    TRACE_MESSAGE("WriteValue: Enter");

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

    TRACE_MESSAGE("WriteValue: Exit");
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
bool BluetoothCharacteristicValue::ReadValue(uint8_t * const dest, const int length)
{
    TRACE_MESSAGE("ReadValue: Enter");

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

    TRACE_MESSAGE("ReadValue: Exit");
}
