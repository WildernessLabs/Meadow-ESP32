/**
 * @file Uuid.hpp
 * @author Chris Tacke
 * 
 * @brief Definiton for the structures holding 16 and 128 bit UUIDs.
 * 
 * @copyright Copyright (c) 2019 - 2024
 */
#ifndef _BLUETOOTH_UUID_HPP_
#define _BLUETOOTH_UUID_HPP_

#include "string.h"
#include "bt_compat.hpp"

#include <string>

/**
 * @brief struct to hold a 128-bit UUID (GUID).
 */
struct Uuid128
{
    /**
     * @brief Data (GUID) held in this structure.
     */
    uint8_t data[16];

    Uuid128(const char *);
    Uuid128();
    Uuid128(uint8_t *other);

    std::string ToString() const;

    /**
     * @brief Construct a new Uuid128 object (copy constructor)
     * 
     * @param other Structure holding the data to be copied.
     */
    Uuid128(const Uuid128 &other)
    {
        memcpy((void *) data, other.data, 16);
    }

    /**
     * @brief Overload the equality operator to compare two Uuid128 objects.
     * 
     * @param other Object to be compared with.
     * @return true If the data in the two structures is the same.
     * @return false If the data in the two structures is different.
     */
    bool operator==(const Uuid128 &other) const
    {
        return memcmp(data, other.data, 16) == 0;
    }

    /**
     * @brief Copy the data from one Uuid128 structure into this structure.
     * 
     * @param other Uuid16 object to copy the data from.
     * @return Uuid16 This structure.
     */
    Uuid128 operator=(const Uuid128 &other) const
    {
        memcpy((void *) data, other.data, 16);
        return other;
    }

    /**
     * @brief Copy the data from 16 bytes of data into the data in this UUID.
     * 
     * @param other Data to be copied into this structure.
     * @return uint8_t* Pointer to the source data.
     */
    uint8_t *operator=(uint8_t *other) const
    {
        memcpy((void *) data, other, 16);
        return other;
    }
};

/**
 * @brief struct to hold a 16-bit UUID.
 */
struct Uuid16
{
    /**
     * @brief Data held in this structure.
     */
    uint8_t data[2];

    /**
     * @brief Overloaded the cast to uint16_t.
     * 
     * @return uint16_t Data in the structure as a uint16_t.
     */
    operator uint16_t() const
    {
        return (uint16_t) *data;
    }

    Uuid16();
    Uuid16(uint16_t value);
    Uuid16(uint8_t *other);

    /**
     * @brief Construct a new Uuid16 object (copy constructor)
     * 
     * @param other Structure holding the data to be copied.
     */
    Uuid16(const Uuid16 &other)
    {
        memcpy((void *) data, other.data, 2);
    }

    /**
     * @brief Overload the equality operator to compare two Uuid16 objects.
     * 
     * @param other Object to be compared with.
     * @return true If the data in the two structures is the same.
     * @return false If the data in the two structures is different.
     */
    bool operator==(const Uuid16 &other) const
    {
        return memcmp(data, other.data, 2) == 0;
    }

    /**
     * @brief Copy the data from one Uuid16 structure into this structure.
     * 
     * @param other Uuid16 object to copy the data from.
     * @return Uuid16 This structure.
     */
    Uuid16 operator=(const Uuid16 &other) const
    {
        memcpy((void *) data, other.data, 2);
        return other;
    }

    /**
     * @brief Copy the data from two bytes of data into the data in this UUID.
     * 
     * @param other Data to be copied into this structure.
     * @return uint8_t* Pointer to the source data.
     */
    uint8_t *operator=(uint8_t *other) const
    {
        memcpy((void *) data, other, 2);
        return other;
    }

    /**
     * @brief Primary Service UUID.
     */
    static Uuid16 PrimaryService;

    /**
     * @brief Characteristic Declaration UUID.
     */
    static Uuid16 CharacteristicDeclaration;
};

#endif
