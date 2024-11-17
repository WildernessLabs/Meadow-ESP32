/**
 * @file BluetoothServiceCollection.cpp
 * @author Chris Tacke
 * 
 * @brief Implementation of the methods in the BluetoothServiceCollection class.
 * 
 * @copyright Copyright (c) 2019 - 2024
 */
#include "BluetoothServiceCollection.hpp"
#include "esp_log.h"

/**
 * @brief Add a new service to the services collection.
 * 
 * @param service Service to be added.
 * @return BluetoothService& 
 */
BluetoothService &BluetoothServiceCollection::Add(BluetoothService *service)
{
    // Do we already know about this service?  If so, ignore
    bool exists = false;

    for (auto it = _services.begin(); it != _services.end(); ++it)
    {
        auto svc = (BluetoothService *) *it;

        if (svc->Uuid == service->Uuid)
        {
            ESP_LOGI(LOG_COMPONENT_NAME, "Already exists.  Skipping\n");
            exists = true;
            break;
        }
    }

    if (!exists)
    {
        _services.push_back(service);
        service->SetHandleManager(this->_handleManager);
        _handleManager->Assign(service);

        // Instance method callback binding.  Yay, c++!
        service->OnCharacteristicAdded = std::bind(&BluetoothServiceCollection::HandleServiceCharacteristicAdded, this, std::placeholders::_1);

        if (OnServiceAdded != NULL)
        {
            OnServiceAdded(service);
        }
    }

    ESP_LOGI(LOG_COMPONENT_NAME, "%i services known\n", (int) _services.size());

    return *service;
}

/**
 * @brief Get the service definitions in this collection.
 * 
 * @return vector<BluetoothService *> Service definitions.
 */
vector<BluetoothService *> BluetoothServiceCollection::GetServiceDefinitions()
{
    return _services;
}

/**
 * @brief Handle the service characteristic added event.
 * 
 * @param characteristic Pointer to the characteristic that has been added to the service.
 */
void BluetoothServiceCollection::HandleServiceCharacteristicAdded(BluetoothCharacteristic *characteristic)
{
    // Pass the event upstream
    characteristic->OnValueChanged = std::bind(&BluetoothServiceCollection::HandleServiceValueChanged, this, std::placeholders::_1);

    if (OnCharacteristicAdded)
    {
        OnCharacteristicAdded(characteristic);
    }
}

/**
 * @brief Handle the service value changed event.
 * 
 * @param characteristic Characteristic that has had its value changed.
 */
void BluetoothServiceCollection::HandleServiceValueChanged(BluetoothCharacteristic *characteristic)
{
    if (OnValueChanged)
    {
        OnValueChanged(characteristic);
    }
}

/**
 * @brief Find a service in the collection by UUID.
 * 
 * @param uuid Pointer to an array of bytes containing the 16-bit UUID fro the service.
 * @return BluetoothService* Pointer to the service if found, NULL otherwise.
 */
BluetoothService *BluetoothServiceCollection::Find(uint8_t *uuid)
{
    return Find(Uuid16(uuid));
}

/**
 * @brief Find a service in the collection by UUID.
 * 
 * @param uuid Uuid16 object containing the UUID of the service to be found.
 * @return BluetoothService* Pointer to the service if found, NULL otherwise.
 */
BluetoothService *BluetoothServiceCollection::Find(Uuid16 uuid)
{
    for (auto it = _services.begin(); it != _services.end(); ++it)
    {
        auto svc = (BluetoothService *) *it;

        if (svc->Uuid == uuid)
        {
            ESP_LOGI(LOG_COMPONENT_NAME, "Found service with matching UUID\n");
            return svc;
        }
    }

    return NULL;
}

/**
 * @brief Get the number of services in this collection.
 * 
 * @return int Number of services in the collection.
 */
int BluetoothServiceCollection::Count()
{
    return _services.size();
}