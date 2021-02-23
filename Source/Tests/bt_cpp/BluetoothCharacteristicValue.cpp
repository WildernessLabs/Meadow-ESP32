#include "BluetoothCharacteristicValue.hpp"
BluetoothCharacteristicValue::BluetoothCharacteristicValue(uint16_t uuid, CharacteristicPermission permission, int maxValueLength)
{
    Uuid = uuid;
    Permission = permission;
    _value = new uint8_t[maxValueLength];
}

BluetoothCharacteristicValue::~BluetoothCharacteristicValue()
{
    if(_value)
    {
        delete _value;
        _value = NULL;
    }
}

bool BluetoothCharacteristicValue::WriteValue(uint8_t* source, int length)
{
    if(length > MaxLength) return false;
    if(length < 0) return false;

    if(length > 0)
    {
        memcpy(_value, source, length);

        // TODO: callback
    }

    return true;
}

bool BluetoothCharacteristicValue::ReadValue(uint8_t* dest, int length)
{
    if(length > CurrentLength) return false;
    if(length < 0) return false;

    if(length > 0)
    {
        memcpy(dest, _value, length);
    }

    return true;
}

