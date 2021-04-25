#ifndef _BLUETOOTH_CHARACTERISTIC_DECLARATION_HPP_
#define _BLUETOOTH_CHARACTERISTIC_DECLARATION_HPP_

#include <string>
#include <vector>

#include "BluetoothAttribute.hpp"
#include "BluetoothCharacteristic.hpp"

using namespace std;

class BluetoothCharacteristicDeclaration : public BluetoothAttribute
{
    public:
        BluetoothCharacteristicDeclaration(uint16_t value_uuid, CharacteristicPermission permission, CharacteristicProperty properties) 
        {
            Uuid = Uuid16::CharacteristicDeclaration;
            Permission = permission;
            Properties = properties;
            ValueUuid = value_uuid;
        }

        Uuid16 Uuid;
        CharacteristicProperty Properties; // A bitfield listing the permitted operations on this characteristic
        uint16_t ValueHandle; // The handle of the attribute containing the characteristic value
        uint16_t ValueUuid;
        uint16_t UUID; // 2, 4, or 16 -byte UUID for this particular characteristic
};

#endif