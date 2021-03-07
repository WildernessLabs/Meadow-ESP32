#ifndef _BLUETOOTH_DESCRIPTOR_HPP_
#define _BLUETOOTH_DESCRIPTOR_HPP_

#include "BluetoothAttribute.hpp"
#include "Uuid.hpp"

class BluetoothDescriptor : public BluetoothAttribute
{
    private:
        BluetoothCharacteristic *_parent;
        
    public: 
        std::string Name;
        Uuid16 Uuid;

        uint16_t MaxLength;
        uint16_t CurrentLength;
        uint8_t *Value;

};


#endif