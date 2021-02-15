#include "BluetoothCharacteristic.hpp"


BluetoothCharacteristic::BluetoothCharacteristic(uint16_t uuid, CharacteristicPermission permission, CharacteristicProperty property)
{
    // a characterisic has both a Declaration and Value.  They point to one another.

    // TODO: a HANDLE needs to get assigned.  Not sure if the caller would send it, or we auto-gen

    // create the declaration attribute
    Declaration = new BluetoothCharacteristicDeclaration(uuid, permission, property);

    // create the value attribute
    // TODO: it's permission might differ from the declaration?  Ugh.
    // TODO: allow a default value to be passed in
    Value = new BluetoothCharacteristicValue(uuid, permission);

    // update the declaration to point to the value
    Declaration->ValueHandle = Value->Handle;
}
