#ifndef _BLUETOOTH_CHARACTERISTIC_VALUE_HPP_
#define _BLUETOOTH_CHARACTERISTIC_VALUE_HPP_

#include <string>
#include <vector>

#include "BluetoothAttribute.hpp"

using namespace std;

class BluetoothCharacteristicValue : public BluetoothAttribute
{
    private:
        uint8_t *_value;

    public:
        
        int MaxLength;
        int CurrentLength;
        uint16_t Uuid;

        BluetoothCharacteristicValue(uint16_t uuid, CharacteristicPermission permission, int maxValueLength);
        ~BluetoothCharacteristicValue();
        bool WriteValue(uint8_t* source, int length);
        bool ReadValue(uint8_t* dest, int length);
};

#endif