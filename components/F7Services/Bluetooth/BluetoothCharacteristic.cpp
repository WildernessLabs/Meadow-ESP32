/**
 * @file BluetoothCharacteristic.cpp
 * @author Chris Tacke
 * @brief Implementation of the BluetoothCharacteristic class.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#include "BluetoothComponentName.hpp"
#include "Logging.hpp"

#include "BluetoothCharacteristic.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

const char *BluetoothCharacteristic::COMPONENT_NAME = BLUETOOTH_COMPONENT_NAME;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

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
    TRACE_MESSAGE("BluetoothCharacteristic(UUID16): Enter");

    // A characteristic has both a Declaration and Value.  They point to one another.

    // TODO: a HANDLE needs to get assigned.  Not sure if the caller would send it, or we auto-gen

    // Create the declaration attribute
    Declaration = new BluetoothCharacteristicDeclaration(uuid, permission, property);

    // Create the value attribute
    // TODO: it's permission might differ from the declaration?  Ugh.
    // TODO: allow a default value to be passed in
    Value = new BluetoothCharacteristicValue(this, uuid, permission, maxValueLength);

    // Update the declaration to point to the value
    Declaration->ValueHandle = Value->Handle;

    TRACE_MESSAGE("BluetoothCharacteristic(UUID16): Exit");
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
    TRACE_MESSAGE("BluetoothCharacteristic(UUID128): Enter");

    // A characteristic has both a Declaration and Value.  They point to one another.

    // TODO: a HANDLE needs to get assigned.  Not sure if the caller would send it, or we auto-gen

    // Create the declaration attribute
    Declaration = new BluetoothCharacteristicDeclaration(uuid, permission, property);

    // Create the value attribute
    // TODO: it's permission might differ from the declaration?  Ugh.
    // TODO: allow a default value to be passed in
    Value = new BluetoothCharacteristicValue(this, uuid, permission, maxValueLength);

    // Update the declaration to point to the value
    Declaration->ValueHandle = Value->Handle;

    TRACE_MESSAGE("BluetoothCharacteristic(UUID128): Exit");
}

/**
 * @brief Destroy the BluetoothCharacteristic object and release any allocated resources.
 */
BluetoothCharacteristic::~BluetoothCharacteristic()
{
    TRACE_MESSAGE("~BluetoothCharacteristic: Enter");

    for (auto it = _descriptors.begin(); it != _descriptors.end(); ++it)
    {
        delete *it;
    }
    _descriptors.clear();

    delete Declaration;
    delete Value;

    TRACE_MESSAGE("~BluetoothCharacteristic: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Add a new descriptor to the characteristic.
 *
 * @param descriptor Descriptor to be added.
 */
void BluetoothCharacteristic::AddDescriptor(BluetoothDescriptor *descriptor)
{
    TRACE_MESSAGE("AddDescriptor: Enter");

    // TODO: do we already know about it?

    // TODO: does it have a handle?

    // TODO: set the parent

    _descriptors.push_back(descriptor);

    // Callback that it was added to the graph
    if (OnDescriptorAdded)
    {
        OnDescriptorAdded(descriptor);
    }

    TRACE_MESSAGE("AddDescriptor: Exit");
}