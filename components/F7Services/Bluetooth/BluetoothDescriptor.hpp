/**
 * @file BluetoothDescriptor.hpp
 * @author Chris Tacke
 * @brief Class definition for the BluetoothDescriptor class.
 * @date 2021-02-14
 * 
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

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
     * @param value Initial value for the descriptor.
     */
    BluetoothDescriptor(BluetoothCharacteristic *parent, Uuid128 uuid, CharacteristicPermission permission, uint16_t maxValueLength, uint8_t *value)
    {
        _parent = parent;
        uuid128 = uuid;
        Permission = permission;
        MaxLength = maxValueLength;
        Value = new uint8_t[MaxLength];
        if (value != NULL)
        {
            memcpy(Value, value, MaxLength);
            CurrentLength = maxValueLength;
        }
        else
        {
            memset(Value, 0, MaxLength);
            CurrentLength = 0;
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
     * @param value Initial value for the descriptor.
     */
    BluetoothDescriptor(BluetoothCharacteristic *parent, Uuid16 uuid, CharacteristicPermission permission, uint16_t maxValueLength, uint8_t *value)
    {
        _parent = parent;
        uuid16 = uuid;
        Permission = permission;
        MaxLength = maxValueLength;
        Value = new uint8_t[MaxLength];
        if (value != NULL)
        {
            memcpy(Value, value, MaxLength);
            CurrentLength = maxValueLength;
        }
        else
        {
            memset(Value, 0, MaxLength);
            CurrentLength = 0;
        }
    }

    /**
     * @brief Destroy the Bluetooth Descriptor object
     */
    ~BluetoothDescriptor()
    {
        delete[] Value;
    }

    /**
     * @brief Set the Value descriptor.
     *
     * @param value Value to set the descriptor to.
     */
    void SetValue(uint32_t length, uint8_t *value)
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