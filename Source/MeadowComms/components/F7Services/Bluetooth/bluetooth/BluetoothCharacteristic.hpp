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

#include "sdkconfig.h"

#include <functional>
#include <string>
#include <vector>

class BluetoothService;
class BluetoothCharacteristic;
class BluetoothCharacteristicValue;

#include "BluetoothDescriptor.hpp"
#include "BluetoothCharacteristicDeclaration.hpp"
#include "BluetoothCharacteristicValue.hpp"

using namespace std;

typedef std::function<void(BluetoothService *)> ServiceAdded;
typedef std::function<void(BluetoothCharacteristic *)> CharacteristicAdded;
typedef std::function<void(BluetoothCharacteristic *)> ValueChanged;
typedef std::function<void(BluetoothDescriptor *)> DescriptorAdded;

/**
 * @brief Class holding a Bluetooth characteristic.
 */
class BluetoothCharacteristic
{
public:
    BluetoothService *ParentService;

    BluetoothCharacteristic(Uuid16 uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength);
    BluetoothCharacteristic(Uuid128 uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength);

    BluetoothCharacteristicDeclaration *Declaration;
    BluetoothCharacteristicValue *Value;

    void AddDescriptor(BluetoothDescriptor *descriptor);

    ValueChanged OnValueChanged = NULL;
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