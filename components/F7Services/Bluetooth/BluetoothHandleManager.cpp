/**
 * @file BluetoothHandleManager.cpp
 * @author Chris Tacke
 *
 * @brief Methods for the BluetoothHandleManager class.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#include "Logging.hpp"

#include "BluetoothHandleManager.hpp"
#include "BluetoothComponentName.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
const char *BluetoothHandleManager::COMPONENT_NAME = BLUETOOTH_COMPONENT_NAME;

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Assign a handle to a service.
 *
 * @param service Service requiring a handle.
 * @return unsigned short Handle assigned to the service.
 */
unsigned short BluetoothHandleManager::Assign(BluetoothService *service)
{
    TRACE_MESSAGE("Assign(BluetoothService): Enter");

    auto h = GetHandle();
    service->Handle = h;
    return h;

    TRACE_MESSAGE("Assign(BluetoothService): Exit");
}

/**
 * @brief Assign a new handle to an attribute.
 *
 * @param attribute Attribute requiring a handle.
 * @return unsigned short Handle assigned to the attribute.
 */
unsigned short BluetoothHandleManager::Assign(BluetoothAttribute *attribute)
{
    TRACE_MESSAGE("Assign(BluetoothAttribute): Enter");

    auto h = GetHandle();
    attribute->Handle = h;
    return h;

    TRACE_MESSAGE("Assign(BluetoothAttribute): Exit");
}
