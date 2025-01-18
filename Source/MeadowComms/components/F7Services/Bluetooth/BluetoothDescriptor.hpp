/**
 * @file BluetoothDescriptor.hpp
 * @author your name (you@domain.com)
 *
 * @brief Class defintion and implementation for the BluetoothDescriptor class.
 *
 * @copyright Copyright (c) 2019 - 2024
 *
 */
#ifndef _BLUETOOTH_DESCRIPTOR_HPP_
#define _BLUETOOTH_DESCRIPTOR_HPP_

// #include "sdkconfig.h"

#include "BluetoothAttribute.hpp"
#include "Uuid.hpp"

/**
 * @brief Data and methods for a Bluetooth descriptor.
 */
class BluetoothDescriptor : public BluetoothAttribute
{
private:
    /**
     * @brief Pointer to the parent (characteristic) of this descriptor.
     */
    BluetoothCharacteristic *_parent;

    /**
     * @brief Is the UUID 128 bits?
     */
    bool _is128 = false;

public:
    /**
     * @brief Construct a new BluetoothDescriptor object.
     *
     * @param parent Characteristic that this descriptor belongs to.
     * @param uuid 128-bit UUID for this descriptor.
     * @param permission Permissions for this descriptor.
     * @param maxValueLength Maximum length of the data in the descriptor.
     */
    BluetoothDescriptor(BluetoothCharacteristic *parent, Uuid128 uuid, CharacteristicPermission permission, uint16_t maxValueLength)
    {
        _parent = parent;
        uuid128 = uuid;
        Permission = permission;
        MaxLength = maxValueLength;
        Value = new uint8_t[MaxLength];
        memset(Value, 0, MaxLength);
        _is128 = true;
    }

    /**
     * @brief Construct a new BluetoothDescriptor object.
     *
     * @param parent Characteristic that this descriptor belongs to.
     * @param uuid 16-bit UUID for this descriptor.
     * @param permission Permissions for this descriptor.
     * @param maxValueLength Maximum length of the data in the descriptor.
     */
    BluetoothDescriptor(BluetoothCharacteristic *parent, Uuid16 uuid, CharacteristicPermission permission, uint16_t maxValueLength)
    {
        _parent = parent;
        uuid16 = uuid;
        Permission = permission;
        MaxLength = maxValueLength;
        Value = new uint8_t[MaxLength];
        memset(Value, 0, MaxLength);
    }

    /**
     * @brief Set the Value descriptor.
     *
     * @param value Value to set the descriptor to.
     */
    void SetValue(int value)
    {
        if (sizeof(int) > MaxLength)
        {
            return;
        }
        memset(Value, 0, MaxLength);
        memcpy(Value, &value, sizeof(int));
    }

    /**
     * @brief Name of the descriptor.
     */
    std::string Name;

    /**
     * @brief Is the UUID a 16 or 128 bit UUID?
     *
     * @return true If the UUID is 128 bits.
     * @return false If the UUID is 16 bits.
     */
    bool Is128()
    {
        return _is128;
    }

    /**
     * @brief 16-bit UUID for this descriptor.
     */
    Uuid16 uuid16;

    /**
     * @brief 128-bit UUID for this descriptor.
     */
    Uuid128 uuid128;

    /**
     * @brief Maximum length of the data in the descriptor.
     */
    uint16_t MaxLength;

    /**
     * @brief Current length of the data in the descriptor.
     */
    uint16_t CurrentLength;

    /**
     * @brief Pointer to the block of memory holding the value.
     */
    uint8_t *Value;
};

#endif