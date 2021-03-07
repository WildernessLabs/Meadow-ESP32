#ifndef _BLUETOOTH_CHARACTERISTIC_VALUE_HPP_
#define _BLUETOOTH_CHARACTERISTIC_VALUE_HPP_

#include <string>
#include <vector>

#include "BluetoothAttribute.hpp"
#include "BluetoothCharacteristic.hpp"

using namespace std;

class BluetoothCharacteristic;

class BluetoothCharacteristicValue : public BluetoothAttribute
{
    private:
        uint8_t *_value;
        BluetoothCharacteristic *_parent;

    public:
        
        uint16_t MaxLength;
        uint16_t CurrentLength;
        uint16_t Uuid;
        BluetoothCharacteristic *Parent;

        BluetoothCharacteristicValue(BluetoothCharacteristic *, uint16_t uuid, CharacteristicPermission permission, int maxValueLength);
        ~BluetoothCharacteristicValue();
        bool WriteValue(uint8_t* source, int length);
        bool ReadValue(uint8_t* dest, int length);
};

#endif