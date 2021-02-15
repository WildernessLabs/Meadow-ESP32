#ifndef _BLUETOOTH_CHARACTERISTIC_VALUE_HPP_
#define _BLUETOOTH_CHARACTERISTIC_VALUE_HPP_

#include <string>
#include <vector>

#include "BluetoothAttribute.hpp"

using namespace std;

class BluetoothCharacteristicValue : public BluetoothAttribute
{
    public:
        byte *Value;
        uint16_t Uuid;

        BluetoothCharacteristicValue(uint16_t uuid, CharacteristicPermission permission)
        {
            // TODO: where does the handle come from?  Is it passed in, or generated?
            
            Uuid = uuid;
            Permission = permission;
            Value = NULL;
        }
};

#endif