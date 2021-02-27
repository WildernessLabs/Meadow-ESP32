#ifndef _BLUETOOTH_CHARACTERISTIC_HPP_
#define _BLUETOOTH_CHARACTERISTIC_HPP_

#include <string>
#include <vector>

class BluetoothService;
class BluetoothCharacteristic;
class BluetoothCharacteristicValue;

#include "BluetoothDescriptor.hpp"
#include "BluetoothCharacteristicDeclaration.hpp"
#include "BluetoothCharacteristicValue.hpp"

using namespace std;

typedef std::function<void(BluetoothService *)> ServiceAdded;
typedef std::function<void(BluetoothCharacteristic *)> CharacteristicAdded;
typedef std::function<void(BluetoothCharacteristic *)> ValueChanged;
typedef std::function<void(BluetoothDescriptor *)> DescriptorAdded;

class BluetoothCharacteristic
{
    public:
        BluetoothService *ParentService;

        BluetoothCharacteristic(uint16_t uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength);

        BluetoothCharacteristicDeclaration *Declaration;
        BluetoothCharacteristicValue * Value;

        void AddDescriptor(BluetoothDescriptor *descriptor);

        ValueChanged OnValueChanged = NULL;
        DescriptorAdded OnDescriptorAdded = NULL;

    
    private:
        vector<BluetoothDescriptor*> _descriptors;
};


#endif