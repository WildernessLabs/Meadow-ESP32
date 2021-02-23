#ifndef _BLUETOOTH_CHARACTERISTIC_HPP_
#define _BLUETOOTH_CHARACTERISTIC_HPP_

#include <string>
#include <vector>

#include "BluetoothDescriptor.hpp"
#include "BluetoothCharacteristicDeclaration.hpp"
#include "BluetoothCharacteristicValue.hpp"

using namespace std;

class BluetoothService;

class BluetoothCharacteristic
{
    public:
        BluetoothService *ParentService;

        BluetoothCharacteristic(uint16_t uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength);

        BluetoothCharacteristicDeclaration *Declaration;
        BluetoothCharacteristicValue * Value;

        void AddDescriptor(BluetoothDescriptor *descriptor);
    
        void SetValue(uint8_t *data, int dataLength);

    private:
        vector<BluetoothDescriptor*> _descriptors;
};

#endif