#ifndef _BLUETOOTH_DESCRIPTOR_HPP_
#define _BLUETOOTH_DESCRIPTOR_HPP_

#include "BluetoothAttribute.hpp"
#include "Uuid.hpp"

class BluetoothDescriptor : public BluetoothAttribute
{
    public: 
        std::string Name;
        Uuid16 Uuid;    
};


#endif