/**
 * @file BluetoothCharacteristic.hpp
 * @author Chris Tacke
 *
 * @brief Class definition for a Bluetooth characteristic.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#ifndef _BLUETOOTH_CHARACTERISTIC_HPP_
#define _BLUETOOTH_CHARACTERISTIC_HPP_

#include <functional>
#include <string>
#include <vector>

//
//  Some forward declarations.
//
class BluetoothService;
class BluetoothCharacteristic;
class BluetoothCharacteristicValue;

#include "BluetoothDescriptor.hpp"
#include "BluetoothCharacteristicDeclaration.hpp"
#include "BluetoothCharacteristicValue.hpp"

using namespace std;

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
    BluetoothCharacteristic(Uuid16 uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength);

    /**
     * @brief Construct a new Bluetooth Characteristic objectwith a 128-bit UUID.
     */
    BluetoothCharacteristic(Uuid128 uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength);

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

    void AddDescriptor(BluetoothDescriptor *descriptor);

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
    vector<BluetoothDescriptor *> GetDescriptorDefinitions()
    {
        return _descriptors;
    }

private:
    /**
     * @brief Bluetooth descriptors for this characteristic.
     */
    vector<BluetoothDescriptor *> _descriptors;
};

#endif