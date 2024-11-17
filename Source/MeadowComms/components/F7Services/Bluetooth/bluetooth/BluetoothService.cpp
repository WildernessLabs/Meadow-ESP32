/**
 * @file BluetoothService.cpp
 * @author Chris Tacke
 * 
 * @brief Implementation for the BluetoothService class.
 * 
 * @copyright Copyright (c) 2019 - 2024
 */
#include "BluetoothService.hpp"
#include "esp_log.h"

/**
 * @brief Construct a new BluetoothService object.
 */
BluetoothService::BluetoothService()
{
}

/**
 * @brief Construct a new BluetoothService object.
 * 
 * @param name Name of the service.
 * 
 * @param uuid UUID of the service.
 */
BluetoothService::BluetoothService(const char *name, Uuid16 uuid)
{
    Name.assign(name);
    Uuid = uuid;
}

/**
 * @brief Add a characteristic to the service.
 * 
 * @param characteristic Characteristic to be added.
 * @return BluetoothCharacteristic& Reference to the characteristic that has been added.
 */
BluetoothCharacteristic &BluetoothService::AddCharacteristic(BluetoothCharacteristic *characteristic)
{
    // do we already know about this characteristic?  If so, ignore
    bool exists = false;

    /*
    for (auto it = _characteristics.begin() ; it != _characteristics.end(); ++it)
    {
        auto chr = (BluetoothCharacteristic*)*it;

        // only check non-zero handles since the handle will be zero when we generate the graph
        if((characteristic->Declaration->Handle != 0) && (chr->Declaration->Handle == characteristic->Declaration->Handle))
        {
            printf("Already exists.  Skipping\n");
            exists = true;
            break;
        }
    }
    */

    if (!exists)
    {
        /*
        // make sure handles are set
        if(characteristic->Declaration->Handle == 0)
        {
            _handleManager->Assign(characteristic->Declaration);
        }
        if(characteristic->Value->Handle == 0)
        {
            _handleManager->Assign(characteristic->Value);
        }

        // cross reference handles
        characteristic->Declaration->ValueHandle = characteristic->Value->Handle;

        */

        // set the parent
        characteristic->ParentService = this;

        _characteristics.push_back(characteristic);

        if (OnCharacteristicAdded)
        {
            OnCharacteristicAdded(characteristic);
        }
    }

    ESP_LOGI(LOG_COMPONENT_NAME, "%i characteristics known\n", (int) _characteristics.size());

    return *characteristic;
}
