/**
 * @file BluetoothHandleManager.hpp
 * @author Chris Tacke
 * @brief Class definition for the BluetoothHandleManager class.
 * @date 2021-02-20
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include "BluetoothService.hpp"
#include "BluetoothAttribute.hpp"
#include <atomic>

class BluetoothService;

/**
 * @brief
 */
class BluetoothHandleManager
{
private:
    // NOTE: this value was arbitrarily decided based on an example.  No idea if it has meaning
    const unsigned short FIRST_HANDLE = 0x0021;

    /**
     * @brief Value GetHandle() returns once every handle has been assigned.  0x0000 is
     *  reserved in the Bluetooth GATT specification and never assigned to a real
     *  attribute, so it doubles as an unambiguous "no handle available" sentinel.
     */
    static const unsigned short INVALID_HANDLE = 0x0000;

    /**
     * @brief Value _next_handle stops at.  Reserved as the exhaustion marker and never
     *  itself handed out, so that incrementing past it - the point at which an
     *  unsigned short would silently wrap to 0x0000 and start colliding with earlier
     *  handles - can never happen.
     */
    static const unsigned short MAXIMUM_HANDLE = 0xFFFF;

    /**
     * @brief Next handle to be returned in the call to GetHandle().
     */
    std::atomic<unsigned short> _next_handle {FIRST_HANDLE};

    /**
     * @brief Get the next handle value.
     *
     * @return unsigned short Handle value, or INVALID_HANDLE if every handle has already
     *  been assigned.
     */
    unsigned short GetHandle()
    {
        unsigned short current = _next_handle.load();
        while (current != MAXIMUM_HANDLE)
        {
            if (_next_handle.compare_exchange_weak(current, static_cast<unsigned short>(current + 1)))
            {
                return current;
            }
        }
        return INVALID_HANDLE;
    }

public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Construct a new BluetoothHandleManager object
     */
    BluetoothHandleManager()
    {
    }

    unsigned short Assign(BluetoothService * const service);
    unsigned short Assign(BluetoothAttribute * const attribute);
};