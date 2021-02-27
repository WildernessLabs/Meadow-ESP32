#ifndef _BLUETOOTH_UUID_HPP_
#define _BLUETOOTH_UUID_HPP_

#include "string.h"
#include "bt_compat.hpp"

/*
128-bit UUID (GUID)
*/
struct Uuid128
{
    uint8_t data[16];

    Uuid128()
    {
        
    }
    
    Uuid128(uint8_t *other)
    {
        memcpy((void*)data, other, 16);
    }

    bool operator==(const Uuid128  &other) const
    {
        return memcmp(data, other.data, 16) == 0;
    }

    Uuid128 operator=(const Uuid128  &other) const
    {
        memcpy((void*)data, other.data, 16);
        return other;
    }

    uint8_t* operator=(uint8_t *other) const
    {
        memcpy((void*)data, other, 16);
        return other;
    }
};

/*
16-bit UUID
*/
struct Uuid16
{
    uint8_t data[2];

    Uuid16()
    {
    }

    Uuid16(uint16_t value)
    {
        memcpy((void*)data, &value, 2);
    }

    bool operator==(const Uuid16  &other) const
    {
        return memcmp(data, other.data, 2) == 0;
    }

    Uuid16 operator=(const Uuid16  &other) const
    {
        memcpy((void*)data, other.data, 2);
        return other;
    }

    uint8_t* operator=(uint8_t *other) const
    {
        memcpy((void*)data, other, 2);
        return other;
    }

    static Uuid16 PrimaryService;
    static Uuid16 CharacteristicDeclaration;
};

#endif
