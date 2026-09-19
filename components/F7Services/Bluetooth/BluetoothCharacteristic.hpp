/**
 * @file BluetoothCharacteristic.hpp
 * @author Chris Tacke
 * @brief Class definition for the BluetoothCharacteristic class.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

//
//  Some forward declarations.
//
class BluetoothService;
class BluetoothCharacteristic;
class BluetoothCharacteristicValue;
class BluetoothCharacteristicDeclaration;

#include "BluetoothDescriptor.hpp"
#include "BluetoothCharacteristicDeclaration.hpp"
#include "BluetoothCharacteristicValue.hpp"

/**
 * @brief Prototype for the ServiceAdded event.
 */
typedef std::function<void(BluetoothService *)> ServiceAdded;

/**
 * @brief Prototype for the CharacteristicAdded event.
 */
typedef std::function<void(BluetoothCharacteristic *)> CharacteristicAdded;

/**
 * @brief Prototype for the ValueChanged event.
 */
typedef std::function<void(BluetoothCharacteristic *)> ValueChanged;

/**
 * @brief Prototype for the DescriptorAdded event.
 */
typedef std::function<void(BluetoothDescriptor *)> DescriptorAdded;

/**
 * @brief Class holding a Bluetooth characteristic.
 */
class BluetoothCharacteristic
{
public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Pointer to the service that owns this characteristic.
     */
    BluetoothService *ParentService;

    /**
     * @brief Construct a new Bluetooth Characteristic object with a 16-bit UUID.
     */
    BluetoothCharacteristic(const Uuid16 uuid, const CharacteristicPermission permission, const CharacteristicProperty property, const int maxValueLength);

    /**
     * @brief Construct a new Bluetooth Characteristic objectwith a 128-bit UUID.
     */
    BluetoothCharacteristic(const Uuid128 uuid, const CharacteristicPermission permission, const CharacteristicProperty property, const int maxValueLength);

    /**
     * @brief Destroy the BluetoothCharacteristic object
     */
    ~BluetoothCharacteristic();

    /**
     * @brief Characteristic declaration.
     */
    BluetoothCharacteristicDeclaration *Declaration;

    /**
     * @brief Characteristic value.
     */
    BluetoothCharacteristicValue *Value;

    void AddDescriptor(BluetoothDescriptor * const descriptor);

    /**
     * @brief Method to be executed when a value changes.
     */
    ValueChanged OnValueChanged = NULL;

    /**
     * @brief Method to be executed when a descriptor is added to the characteristic.
     */
    DescriptorAdded OnDescriptorAdded = NULL;

    /**
     * @brief Get the vector of BluetoothDescriptor objects for this characteristic.
     *
     * @return vector<BluetoothDescriptor *> Vector of BluetoothDescriptor objects.
     */
    std::vector<BluetoothDescriptor *> GetDescriptorDefinitions();

private:
    /**
     * @brief Bluetooth descriptors for this characteristic.
     */
    std::vector<BluetoothDescriptor *> _descriptors;
};