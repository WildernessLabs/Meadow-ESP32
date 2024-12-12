/**
 * @file BluetoothServiceCollection.hpp
 * @author Wilderness Labs (Chris Tacke)
 * @brief Hold a collection of services known to this Bluetooth device.
 * @version 0.1
 * @date 2024-12-12
 * 
 * @copyright Copyright (c) 2024
 */
#ifndef _BLUETOOTH_SERVICE_COLLECTION_HPP_
#define _BLUETOOTH_SERVICE_COLLECTION_HPP_

#include <string>
#include <vector>

#include "Uuid.hpp"
#include "BluetoothService.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothHandleManager.hpp"

using namespace std;

/**
 * @brief Collection of Bluetooth services known for this device.
 */
class BluetoothServiceCollection
{
    private:
        /**
         * @brief Collection of services.
         */
        vector<BluetoothService*> _services;

        /**
         * @brief Handle manager for the services in the collection.
         */
        BluetoothHandleManager* _handleManager;

        /**
         * @brief Process the addition of a new characteristic.
         */
        void HandleServiceCharacteristicAdded(BluetoothCharacteristic *);

        /**
         * @brief Process the value changed event for a characteristic.
         */
        void HandleServiceValueChanged(BluetoothCharacteristic *characteristic);

    public:
        /**
         * @brief Construct a new Bluetooth Service Collection object
         * 
         * @param handleManager Handle manger to be used for this collection.
         */
        BluetoothServiceCollection(BluetoothHandleManager *handleManager)
        {
            _handleManager = handleManager;
        }

        /**
         * @brief Callback for when a service is added.
         */
        ServiceAdded OnServiceAdded = NULL;

        /**
         * @brief Callback for when a characteristic is added.
         */ 
        CharacteristicAdded OnCharacteristicAdded = NULL;

        /**
         * @brief Callback for when a value changes.
         */
        ValueChanged OnValueChanged = NULL;

        /**
         * @brief Add a new service to the collection of known services.
         */
        BluetoothService& Add(BluetoothService *service);

        /**
         * @brief Find the specified service by UUID.
         */
        BluetoothService* Find(uint8_t *uuid);

        /**
         * @brief Find the specified service by UUID.
         */

        BluetoothService* Find(Uuid128 uuid);

        /**
         * @brief Get the number of services in the collection.
         */
        int Count();
};

#endif