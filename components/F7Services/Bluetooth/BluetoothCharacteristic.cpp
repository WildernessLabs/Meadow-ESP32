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
BluetoothCharacteristic::BluetoothCharacteristic(const Uuid16 uuid, const CharacteristicPermission permission, const CharacteristicProperty property, const int maxValueLength) : _declaration(uuid, permission, property), _value(this, uuid, permission, maxValueLength)
{
    TRACE_MESSAGE("BluetoothCharacteristic(UUID16): Enter");

    // A characteristic has both a Declaration and Value.  They point to one another.

    // TODO: a HANDLE needs to get assigned.  Not sure if the caller would send it, or we auto-gen

    // TODO: the value's permission might differ from the declaration's.
    // TODO: allow a default value to be passed in

    // Update the declaration to point to the value
    _declaration.ValueHandle = _value.Handle;

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
BluetoothCharacteristic::BluetoothCharacteristic(const Uuid128 uuid, const CharacteristicPermission permission, const CharacteristicProperty property, const int maxValueLength) : _declaration(uuid, permission, property), _value(this, uuid, permission, maxValueLength)
{
    TRACE_MESSAGE("BluetoothCharacteristic(UUID128): Enter");

    // A characteristic has both a Declaration and Value.  They point to one another.

    // TODO: a HANDLE needs to get assigned.  Not sure if the caller would send it, or we auto-gen

    // TODO: the value's permission might differ from the declaration's.
    // TODO: allow a default value to be passed in

    // Update the declaration to point to the value
    _declaration.ValueHandle = _value.Handle;

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

    TRACE_MESSAGE("~BluetoothCharacteristic: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                              Accessors
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Get the characteristic declaration.
 *
 *  The declaration belongs to the characteristic and lives as long as it does.  The
 *  reference is for reading the declaration and assigning its handles, not for keeping it.
 *
 * @return BluetoothCharacteristicDeclaration& The declaration.
 */
BluetoothCharacteristicDeclaration &BluetoothCharacteristic::GetDeclaration()
{
    return (_declaration);
}

/**
 * @brief Get the characteristic declaration.
 *
 * @return const BluetoothCharacteristicDeclaration& The declaration.
 */
const BluetoothCharacteristicDeclaration &BluetoothCharacteristic::GetDeclaration() const
{
    return (_declaration);
}

/**
 * @brief Get the characteristic value.
 *
 *  The value belongs to the characteristic and lives as long as it does.  The reference is
 *  for reading the value and assigning its handle, not for keeping it.
 *
 * @return BluetoothCharacteristicValue& The value.
 */
BluetoothCharacteristicValue &BluetoothCharacteristic::GetValue()
{
    return (_value);
}

/**
 * @brief Get the characteristic value.
 *
 * @return const BluetoothCharacteristicValue& The value.
 */
const BluetoothCharacteristicValue &BluetoothCharacteristic::GetValue() const
{
    return (_value);
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
void BluetoothCharacteristic::AddDescriptor(BluetoothDescriptor * const descriptor)
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

/**
 * @brief Get the vector of BluetoothDescriptor objects for this characteristic.
 *
 * @return vector<BluetoothDescriptor *> Vector of BluetoothDescriptor objects.
 */
std::vector<BluetoothDescriptor *> BluetoothCharacteristic::GetDescriptorDefinitions()
{
    return _descriptors;
}
