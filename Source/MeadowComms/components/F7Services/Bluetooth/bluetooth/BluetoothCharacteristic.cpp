/**
 * @file BluetoothCharacteristic.cpp
 * @author Chris Tacke
 * 
 * @brief Implementation of methods for the BluetoothCharacteristic class.
 * 
 * @copyright Copyright (c) 2019 - 2024
 */
#include "BluetoothCharacteristic.hpp"

/**
 * @brief Construct a new BluetoothCharacteristic object.
 * 
 * @param uuid 16-bit UUID of this characteristic.
 * @param permission Permissions to be applied to the characteristic.
 * @param property Characteristic properties.
 * @param maxValueLength Maximum length of he value data.
 */
BluetoothCharacteristic::BluetoothCharacteristic(Uuid16 uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength)
{
    // A characteristic has both a Declaration and Value.  They point to one another.

    // TODO: a HANDLE needs to get assigned.  Not sure if the caller would send it, or we auto-gen

    // create the declaration attribute
    Declaration = new BluetoothCharacteristicDeclaration(uuid, permission, property);

    // Create the value attribute
    // TODO: it's permission might differ from the declaration?  Ugh.
    // TODO: allow a default value to be passed in
    Value = new BluetoothCharacteristicValue(this, uuid, permission, maxValueLength);

    // Update the declaration to point to the value
    Declaration->ValueHandle = Value->Handle;
}

/**
 * @brief Construct a new BluetoothCharacteristic object.
 * 
 * @param uuid 128-bit UUID (GUID) of this characteristic.
 * @param permission Permissions to be applied to the characteristic.
 * @param property Characteristic properties.
 * @param maxValueLength Maximum length of he value data.
 */
BluetoothCharacteristic::BluetoothCharacteristic(Uuid128 uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength)
{
    // A characteristic has both a Declaration and Value.  They point to one another.

    // TODO: a HANDLE needs to get assigned.  Not sure if the caller would send it, or we auto-gen

    // create the declaration attribute
    Declaration = new BluetoothCharacteristicDeclaration(uuid, permission, property);

    // Create the value attribute
    // TODO: it's permission might differ from the declaration?  Ugh.
    // TODO: allow a default value to be passed in
    Value = new BluetoothCharacteristicValue(this, uuid, permission, maxValueLength);

    // Update the declaration to point to the value
    Declaration->ValueHandle = Value->Handle;
}

/**
 * @brief Add a new descriptor to the characteristic.
 * 
 * @param descriptor Descriptor to be added.
 */
void BluetoothCharacteristic::AddDescriptor(BluetoothDescriptor *descriptor)
{
    // TODO: do we already know about it?

    // TODO: does it have a handle?

    // TODO: set the parent

    _descriptors.push_back(descriptor);

    // Callback that it was added to the graph
    if (OnDescriptorAdded)
    {
        OnDescriptorAdded(descriptor);
    }
}