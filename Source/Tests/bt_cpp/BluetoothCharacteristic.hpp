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
    private:
        BluetoothService *ParentService;

    public:

        BluetoothCharacteristic(uint16_t uuid, CharacteristicPermission permission, CharacteristicProperty property);

        BluetoothCharacteristicDeclaration *Declaration;
        BluetoothCharacteristicValue * Value;

        vector<BluetoothDescriptor> Descriptors;
};

#endif