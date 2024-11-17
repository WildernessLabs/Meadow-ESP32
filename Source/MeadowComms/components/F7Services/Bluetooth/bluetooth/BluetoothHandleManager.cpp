/**
 * @file BluetoothHandleManager.cpp
 * @author Chris Tacke
 * 
 * @brief Methods for the BluetoothHandleManager class.
 * 
 * @copyright Copyright (c) 2019 - 2024
 */
#include "BluetoothHandleManager.hpp"

/**
 * @brief Assign a handle to a service.
 * 
 * @param service Service requiring a handle.
 * @return unsigned short Handle assigned to the service.
 */
unsigned short BluetoothHandleManager::Assign(BluetoothService *service)
{
    auto h = GetHandle();
    service->Handle = h;
    return h;
}

/**
 * @brief Assign a new handle to an attribute.
 * 
 * @param attribute Attribute requiring a handle.
 * @return unsigned short Handle assigned to the attribute.
 */
unsigned short BluetoothHandleManager::Assign(BluetoothAttribute *attribute)
{
    auto h = GetHandle();
    attribute->Handle = h;
    return h;
}
