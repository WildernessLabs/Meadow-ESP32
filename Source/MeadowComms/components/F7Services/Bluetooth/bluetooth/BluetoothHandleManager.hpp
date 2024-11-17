/**
 * @file BluetoothHandleManager.hpp
 * @author Chris Tacke
 *
 * @brief Class definition for the BluetoothHandleManager class.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#ifndef _BLUETOOTH_HANDLE_MANAGER_HPP_
#define _BLUETOOTH_HANDLE_MANAGER_HPP_

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
     * @brief Construct a new BluetoothHandleManager object
     */
    BluetoothHandleManager()
    {
    }

    unsigned short Assign(BluetoothService *service);
    unsigned short Assign(BluetoothAttribute *attribute);
};

#endif