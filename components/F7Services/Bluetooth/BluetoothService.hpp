/**
 * @file BluetoothService.hpp
 * @author Chris Tacke
 * @brief Class definition for the BluetoothService class.
 * @date 2021-02-14
 * 
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include <string>
#include <vector>

#include "Uuid.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothHandleManager.hpp"

using namespace std;

class BluetoothHandleManager;

/**
 * @brief Class holding the data and methods for a Bluetooth service.
 */
class BluetoothService
{
public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Construct a new Bluetooth Service object
     */
    BluetoothService();

    /**
     * @brief Construct a new Bluetooth Service object
     */
    BluetoothService(const char *name, Uuid16 uuid);

    /**
     * @brief Destroy the BluetoothService object and release any allocated resources.
     */
    ~BluetoothService();

    /**
     * @brief Add a characteristic to the service.
     */
    BluetoothCharacteristic &AddCharacteristic(BluetoothCharacteristic *characteristic);

    /**
     * @brief Event handler for dealing with the addition of a characteristic.
     */
    CharacteristicAdded OnCharacteristicAdded = NULL;

    /**
     * @brief Name of the service.
     */
    std::string Name;

    /**
     * @brief UUID of the service.
     */
    Uuid16 Uuid;

    /**
     * @brief Handle for the service.
     */
    unsigned short Handle;

    /**
     * @brief Set the Bluetooth handle manager.
     *
     * @param handleManager Pointer to the Bluetooth handle manager.
     */
    void SetHandleManager(BluetoothHandleManager *handleManager)
    {
        _handleManager = handleManager;
    }

    /**
     * @brief Get the characteristic definitions for this service.
     *
     * @return vector<BluetoothCharacteristic *> Pointer to the vector of BluetoothCharacteristic objects.
     */
    vector<BluetoothCharacteristic *> GetCharacteristicDefinitions()
    {
        return _characteristics;
    }

private:
    /**
     * @brief Characteristics for this service.
     */
    vector<BluetoothCharacteristic *> _characteristics;

    /**
     * @brief Bluetooth handle manager for this service.
     */
    BluetoothHandleManager *_handleManager;
};