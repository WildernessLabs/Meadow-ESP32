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
#include "BluetoothCharacteristic.hpp"
#include "Uuid.hpp"

/**
 * @brief Data and methods for a Bluetooth descriptor.
 */
class BluetoothDescriptor : public BluetoothAttribute
{
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
    BluetoothDescriptor(const BluetoothCharacteristic * const parent, const Uuid128 uuid, const CharacteristicPermission permission, const uint16_t maxValueLength, const uint8_t * const value);

    /**
     * @brief Construct a new BluetoothDescriptor object.
     *
     * @param parent Characteristic that this descriptor belongs to.
     * @param uuid 16-bit UUID for this descriptor.
     * @param permission Permissions for this descriptor.
     * @param maxValueLength Maximum length of the data in the descriptor.
     * @param value Initial value for the descriptor.
     */
    BluetoothDescriptor(const BluetoothCharacteristic * const parent, const Uuid16 uuid, const CharacteristicPermission permission, const uint16_t maxValueLength, const uint8_t * const value);

    /**
     * @brief Destroy the Bluetooth Descriptor object
     */
    ~BluetoothDescriptor();

    /**
     * @brief Set the Value descriptor.
     *
     * @param value Value to set the descriptor to.
     */
    void SetValue(const uint32_t length, const uint8_t * const value);

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
    bool Is128();

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

private:
    /**
     * @brief Pointer to the parent (characteristic) of this descriptor.
     */
    const BluetoothCharacteristic *_parent;

    /**
     * @brief Is the UUID 128 bits?
     */
    bool _is128 = false;

    /**
     * @brief Deleted copy constructor to prevent copying.
     */
    BluetoothDescriptor(const BluetoothDescriptor &other) = delete;

    /**
     * @brief Deleted copy assignment operator to prevent assignment.
     */
    BluetoothDescriptor &operator=(const BluetoothDescriptor &other) = delete;

    /**
     * @brief Deleted move constructor to prevent moving.
     */
    BluetoothDescriptor(BluetoothDescriptor &&other) = delete;

    /**
     * @brief Deleted move assignment operator to prevent move assignment.
     */
    BluetoothDescriptor &operator=(BluetoothDescriptor &&other) = delete;
};