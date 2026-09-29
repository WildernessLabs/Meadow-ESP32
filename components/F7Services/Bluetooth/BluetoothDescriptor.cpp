/**
 * @file BluetoothDescriptor.cpp
 * @author Mark Stevens
 * @brief Implementation for the BluetoothDescriptor class.
 * @date 2025-12-01
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#include "BluetoothAttribute.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothDescriptor.hpp"
#include "Uuid.hpp"

#include <algorithm>

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Construct a new BluetoothDescriptor object.
 *
 * @param parent Characteristic that this descriptor belongs to.
 * @param uuid 128-bit UUID for this descriptor.
 * @param permission Permissions for this descriptor.
 * @param maxValueLength Maximum length of the data in the descriptor.
 * @param value Initial value for the descriptor, or nullptr for an empty value.
 * @param valueLength Number of bytes available at value; at most maxValueLength of them are copied.
 */
BluetoothDescriptor::BluetoothDescriptor(const BluetoothCharacteristic * const parent, const Uuid128 uuid, const CharacteristicPermission permission, const uint16_t maxValueLength, const uint8_t * const value, const uint16_t valueLength)
{
    _parent = parent;
    uuid128 = uuid;
    Permission = permission;
    MaxLength = maxValueLength;
    Value = new uint8_t[MaxLength];
    memset(Value, 0, MaxLength);
    CurrentLength = 0;
    if (value != nullptr)
    {
        CurrentLength = std::min(valueLength, MaxLength);
        memcpy(Value, value, CurrentLength);
    }
    _is128 = true;
}

/**
 * @brief Construct a new BluetoothDescriptor object.
 *
 * @param parent Characteristic that this descriptor belongs to.
 * @param uuid 16-bit UUID for this descriptor.
 * @param permission Permissions for this descriptor.
 * @param maxValueLength Maximum length of the data in the descriptor.
 * @param value Initial value for the descriptor, or nullptr for an empty value.
 * @param valueLength Number of bytes available at value; at most maxValueLength of them are copied.
 */
BluetoothDescriptor::BluetoothDescriptor(const BluetoothCharacteristic * const parent, const Uuid16 uuid, const CharacteristicPermission permission, const uint16_t maxValueLength, const uint8_t * const value, const uint16_t valueLength)
{
    _parent = parent;
    uuid16 = uuid;
    Permission = permission;
    MaxLength = maxValueLength;
    Value = new uint8_t[MaxLength];
    memset(Value, 0, MaxLength);
    CurrentLength = 0;
    if (value != nullptr)
    {
        CurrentLength = std::min(valueLength, MaxLength);
        memcpy(Value, value, CurrentLength);
    }
}

/**
 * @brief Destroy the Bluetooth Descriptor object
 */
BluetoothDescriptor::~BluetoothDescriptor()
{
    delete[] Value;
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Set the Value descriptor.
 *
 * @param value Value to set the descriptor to.
 */
void BluetoothDescriptor::SetValue(const uint32_t length, const uint8_t * const value)
{
    if ((length > MaxLength) || (value == NULL))
    {
        return;
    }
    memset(Value, 0, MaxLength);
    memcpy(Value, value, length);
    CurrentLength = length;
}

/**
 * @brief Is the UUID a 16 or 128 bit UUID?
 *
 * @return true If the UUID is 128 bits.
 * @return false If the UUID is 16 bits.
 */
bool BluetoothDescriptor::Is128()
{
    return _is128;
}