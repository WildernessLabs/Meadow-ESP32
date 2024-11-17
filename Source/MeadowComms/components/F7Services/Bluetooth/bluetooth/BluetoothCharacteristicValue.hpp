/**
 * @file BluetoothCharacteristicValue.hpp
 * @author Chris Tacke
 *
 * @brief Definition for the BluetoothCharacteristicValue class.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#ifndef _BLUETOOTH_CHARACTERISTIC_VALUE_HPP_
#define _BLUETOOTH_CHARACTERISTIC_VALUE_HPP_

#include "sdkconfig.h"

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
private:
    bool _is128 = false;
    uint8_t *_value;
    BluetoothCharacteristic *_parent;

public:
    uint16_t MaxLength;
    uint16_t CurrentLength;
    Uuid16 uuid16;
    Uuid128 uuid128;
    BluetoothCharacteristic *Parent;

    BluetoothCharacteristicValue() = delete;
    BluetoothCharacteristicValue(BluetoothCharacteristic *c, Uuid16 uuid, CharacteristicPermission permission, int maxValueLength);
    BluetoothCharacteristicValue(BluetoothCharacteristic *c, Uuid128 uuid, CharacteristicPermission permission, int maxValueLength);
    ~BluetoothCharacteristicValue();
    bool WriteValue(uint8_t *source, int length);
    bool ReadValue(uint8_t *dest, int length);

    /**
     *  @brief Indicate if the UUID value is 128-bit.
     *
     *  @return True if the UUID value is 128-bit, false otherwise.
     */
    bool Is128()
    {
        return _is128;
    }
};

#endif