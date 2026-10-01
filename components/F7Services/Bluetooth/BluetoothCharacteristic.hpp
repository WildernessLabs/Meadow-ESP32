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
     * @brief Deleted copy constructor, a copy would share and then double free the owned storage.
     */
    BluetoothCharacteristic(const BluetoothCharacteristic &other) = delete;

    /**
     * @brief Deleted copy assignment operator, a copy would share and then double free the owned storage.
     */
    BluetoothCharacteristic &operator=(const BluetoothCharacteristic &other) = delete;

    /**
     * @brief Deleted move constructor to prevent moving.
     */
    BluetoothCharacteristic(BluetoothCharacteristic &&other) = delete;

    /**
     * @brief Deleted move assignment operator to prevent move assignment.
     */
    BluetoothCharacteristic &operator=(BluetoothCharacteristic &&other) = delete;

    /**
     * @brief Get the characteristic declaration.
     */
    BluetoothCharacteristicDeclaration &GetDeclaration();

    /**
     * @brief Get the characteristic declaration.
     */
    const BluetoothCharacteristicDeclaration &GetDeclaration() const;

    /**
     * @brief Get the characteristic value.
     */
    BluetoothCharacteristicValue &GetValue();

    /**
     * @brief Get the characteristic value.
     */
    const BluetoothCharacteristicValue &GetValue() const;

    void AddDescriptor(BluetoothDescriptor * const descriptor);

    /**
     * @brief Method to be executed when a value changes.
     */
    ValueChanged OnValueChanged = nullptr;

    /**
     * @brief Method to be executed when a descriptor is added to the characteristic.
     */
    DescriptorAdded OnDescriptorAdded = nullptr;

    /**
     * @brief Get the vector of BluetoothDescriptor objects for this characteristic.
     *
     * @return vector<BluetoothDescriptor *> Vector of BluetoothDescriptor objects.
     */
    std::vector<BluetoothDescriptor *> GetDescriptorDefinitions();

private:
    /**
     * @brief Characteristic declaration.
     *
     *  Held as a member, not through a pointer, so its lifetime is the characteristic's and
     *  nothing outside can release it (issue #449).  Callers reach it through GetDeclaration.
     */
    BluetoothCharacteristicDeclaration _declaration;

    /**
     * @brief Characteristic value.
     *
     *  Held as a member for the same reason as _declaration.  It is declared after
     *  _declaration, so it is constructed after it.
     */
    BluetoothCharacteristicValue _value;

    /**
     * @brief Bluetooth descriptors for this characteristic.
     */
    std::vector<BluetoothDescriptor *> _descriptors;
};