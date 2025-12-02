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
     * @brief Next handle to be returned in the call to GetHandle().
     */
    std::atomic<unsigned short> _next_handle {FIRST_HANDLE};

    /**
     * @brief Get the next handle value.
     *
     * @return unsigned short Handle value.
     */
    unsigned short GetHandle()
    {
        return _next_handle++;
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