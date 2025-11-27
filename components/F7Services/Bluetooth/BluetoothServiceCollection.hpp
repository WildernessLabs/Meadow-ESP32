/**
 * @file BluetoothServiceCollection.hpp
 * @author Chris Tacke
 * @brief Class definition for the BluetoothServiceCollection class.
 * @date 2021-02-14
 * 
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include <string>
#include <vector>
#include <functional>

#include "Uuid.hpp"
#include "BluetoothService.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothHandleManager.hpp"

using namespace std;

/**
 * @brief Class holding the data and methods for the collection of Bluetooth services.
 */
class BluetoothServiceCollection
{
private:
    /**
     * @brief Collection of Bluetooth services.
     */
    vector<BluetoothService *> _services;

    /**
     * @brief Pointer to the Bluetooth handle manager.
     */
    BluetoothHandleManager *_handleManager;

    void HandleServiceCharacteristicAdded(BluetoothCharacteristic *);
    void HandleServiceValueChanged(BluetoothCharacteristic *characteristic);

public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Construct a new Bluetooth Service Collection object
     *
     * @param handleManager Handle manager for this service collection.
     */
    BluetoothServiceCollection(BluetoothHandleManager *handleManager)
    {
        _handleManager = handleManager;
    }

    /**
     * @brief Delete all of the registered services.
     */
    bool DeleteAllServices();

    /**
     * @brief ServiceAdded event handler.
     */
    ServiceAdded OnServiceAdded = NULL;

    /**
     * @brief CharacteristicAdded event handler.
     */
    CharacteristicAdded OnCharacteristicAdded = NULL;

    /**
     * @brief ValueChanged event handler.
     */
    ValueChanged OnValueChanged = NULL;

    /**
     * @brief Add a new service to the collection.
     */
    BluetoothService &Add(BluetoothService *service);

    /**
     * @brief Find a service in the collection by UUID.
     */
    BluetoothService *Find(uint8_t *uuid);

    /**
     * @brief Find a service in the collection by UUID.
     */
    BluetoothService *Find(Uuid16 uuid);

    /**
     * @brief Get the number of services in the collection.
     */
    int Count();

    /**
     * @brief Get the service definitions for this collection.
     */
    vector<BluetoothService *> GetServiceDefinitions();
};