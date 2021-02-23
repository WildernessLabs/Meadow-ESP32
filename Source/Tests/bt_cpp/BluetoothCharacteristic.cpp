#include "BluetoothCharacteristic.hpp"


BluetoothCharacteristic::BluetoothCharacteristic(uint16_t uuid, CharacteristicPermission permission, CharacteristicProperty property, int maxValueLength)
{
    // a characterisic has both a Declaration and Value.  They point to one another.

    // TODO: a HANDLE needs to get assigned.  Not sure if the caller would send it, or we auto-gen

    // create the declaration attribute
    Declaration = new BluetoothCharacteristicDeclaration(uuid, permission, property);

    // create the value attribute
    // TODO: it's permission might differ from the declaration?  Ugh.
    // TODO: allow a default value to be passed in
    Value = new BluetoothCharacteristicValue(uuid, permission, maxValueLength);

    // update the declaration to point to the value
    Declaration->ValueHandle = Value->Handle;
}

void BluetoothCharacteristic::AddDescriptor(BluetoothDescriptor *descriptor)
{
    // TODO: do we already know about it?

    // TODO: does it have a handle?

    // TODO: callback that it was added to the graph

    _descriptors.push_back(descriptor);
}

void BluetoothCharacteristic::SetValue(uint8_t *data, int dataLength)
{
    // TODO: will the value fit?

    // TODO: set the value

    // TODO: CALLBACK
}
