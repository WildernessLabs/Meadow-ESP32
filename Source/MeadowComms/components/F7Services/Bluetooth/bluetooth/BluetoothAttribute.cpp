/**
 * @file BluetoothAttribute.cpp
 * @author Mark Stevens
 *
 * @brief Implementation for the BluetoothAttribute class.
 *
 * @copyright Copyright (c) 2024
 */

#include "sdkconfig.h"

#include "BluetoothRequestHandler.hpp"

#include "BluetoothAttribute.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                     Initialise static data.
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component / task.
 */
const char *BluetoothAttribute::COMPONENT_NAME = BluetoothRequestHandler::COMPONENT_NAME;
