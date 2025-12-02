/**
 * @file BluetoothCharacteristicValue.hpp
 * @author Chris Tacke
 * @brief Class definition for the BluetoothCharacteristicValue class.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include <string>
#include <vector>

#include "Uuid.hpp"
#include "BluetoothAttribute.hpp"
#include "BluetoothCharacteristic.hpp"

using namespace std;

class BluetoothCharacteristic;

/**
 * @brief Class holding a Bluetooth characteristic value.
 */
class BluetoothCharacteristicValue : public BluetoothAttribute
{
public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Maximum length of the binary data in this characteristic value.
     */
    uint16_t MaxLength;

    /**
     * @brief Current length of the data in the characteristic value.
     */
    uint16_t CurrentLength;

    /**
     * @brief 16-bit UUID of the characteristic value.
     */
    Uuid16 uuid16;

    /**
     * @brief 128-bit UUID of the characteristic value.
     */
    Uuid128 uuid128;

    /**
     * @brief Characteristic that this value belongs to.
     */
    BluetoothCharacteristic *Parent;

    /**
     * @brief Remove the default constructor.
     */
    BluetoothCharacteristicValue() = delete;

    /**
     * @brief Deleted copy constructor to prevent copying.
     */
    BluetoothCharacteristicValue(const BluetoothCharacteristicValue &other) = delete;

    /**
     * @brief Deleted copy assignment operator to prevent assignment.
     */
    BluetoothCharacteristicValue &operator=(const BluetoothCharacteristicValue &other) = delete;

    /**
     * @brief Deleted move constructor to prevent moving.
     */
    BluetoothCharacteristicValue(BluetoothCharacteristicValue &&other) = delete;

    /**
     * @brief Deleted move assignment operator to prevent move assignment.
     */
    BluetoothCharacteristicValue &operator=(BluetoothCharacteristicValue &&other) = delete;

    /**
     * @brief Construct a new BluetoothCharacteristicValue object with a 16-bit UUID.
     */
    BluetoothCharacteristicValue(BluetoothCharacteristic * const c, const Uuid16 uuid, const CharacteristicPermission permission, const int maxValueLength);

    /**
     * @brief Construct a new BluetoothCharacteristicValue object with a 128-bit UUID.
     */
    BluetoothCharacteristicValue(BluetoothCharacteristic * const c, const Uuid128 uuid, const CharacteristicPermission permission, const int maxValueLength);

    /**
     * @brief Destroy the BluetoothCharacteristicValue object and release any allocated resources.
     */
    ~BluetoothCharacteristicValue();

    /**
     * @brief Set the value of the characteristic.
     */
    bool WriteValue(const uint8_t * const source, const int length);

    /**
     * @brief Read the value of the characteristic.
     */
    bool ReadValue(uint8_t * const dest, const int length);

    /**
     *  @brief Indicate if the UUID value is 128-bit.
     *
     *  @return True if the UUID value is 128-bit, false otherwise.
     */
    bool Is128()
    {
        return _is128;
    }

private:
    /**
     * @brief Is the UUID 128 bits?
     */
    bool _is128 = false;

    /**
     * @brief Pointer ti the memory holding the value.
     */
    uint8_t *_value;

    /**
     * @brief Pointer to the parent (characteristic) of this value.
     */
    BluetoothCharacteristic *_parent;
};