#ifndef _BLUETOOTH_ATTRIBUTE_HPP_
#define _BLUETOOTH_ATTRIBUTE_HPP_

#include "bt_compat.hpp"

enum class CharacteristicProperty : unsigned char {
    Broadcast = 1 << 0,
    Read = 1 << 1,
    WriteNoResponse = 1 << 2,
    Write = 1 << 3,
    Notify = 1 << 4,
    Indicate = 1 << 5,
    SignedWrite = 1 << 6,
    ExtendedProp = 1 << 7
};


enum class CharacteristicPermission : unsigned short {
    Read = 1 << 0,
    ReadEncrypted = 1 << 1,
    ReadEncMITM = 1 << 2,
    // WHERE IS 1 << 3?
    Write = 1 << 4,
    WriteEncrypted = 1 << 5,
    WriteEncMITM = 1 << 6,
    WriteSigned = 1 << 7,
    WriteSignedMITM = 1 << 8
};

class BluetoothAttribute
{
    public:
        CharacteristicPermission Permission;
        uint16_t Handle;
};

#endif