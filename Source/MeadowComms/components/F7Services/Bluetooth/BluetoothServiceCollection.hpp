/**
 * @file BluetoothServiceCollection.hpp
 * @author Chris Tacke
 *
 * @brief Definition for the BluetoothServiceCollection class.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#ifndef _BLUETOOTH_SERVICE_COLLECTION_HPP_
#define _BLUETOOTH_SERVICE_COLLECTION_HPP_

#include "sdkconfig.h"

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

    ServiceAdded OnServiceAdded = NULL;
    CharacteristicAdded OnCharacteristicAdded = NULL;
    ValueChanged OnValueChanged = NULL;

    BluetoothService &Add(BluetoothService *service);
    BluetoothService *Find(uint8_t *uuid);
    BluetoothService *Find(Uuid16 uuid);
    int Count();

    vector<BluetoothService *> GetServiceDefinitions();
};

#endif