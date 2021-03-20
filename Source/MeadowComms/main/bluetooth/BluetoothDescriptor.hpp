#ifndef _BLUETOOTH_DESCRIPTOR_HPP_
#define _BLUETOOTH_DESCRIPTOR_HPP_

#include "BluetoothAttribute.hpp"
#include "Uuid.hpp"

class BluetoothDescriptor : public BluetoothAttribute
{
    private:
        BluetoothCharacteristic *_parent;
        
    public: 
        BluetoothDescriptor(BluetoothCharacteristic *parent, uint16_t uuid, CharacteristicPermission permission, uint16_t maxValueLength)
        {
            _parent = parent;
            Uuid = uuid;
            Permission = permission;
            MaxLength = maxValueLength;
        }

        std::string Name;
        uint16_t Uuid;

        uint16_t MaxLength;
        uint16_t CurrentLength;
        uint8_t *Value;

};


#endif