#include "BluetoothServiceCollection.hpp"
#include "Logging.hpp"

/**
 * @brief Add a new service to the collection of known services.
 * 
 * @param service Service to be added.
 * @return BluetoothService& Service that has been added.
 */
BluetoothService& BluetoothServiceCollection::Add(BluetoothService *service)
{
    // do we already know about this service?  If so, ignore 
    bool exists = false;

    for (auto it = _services.begin() ; it != _services.end(); ++it)
    {
        auto svc = (BluetoothService *) *it;

        if (svc->Uuid == service->Uuid)
        {
            TRACE_MESSAGE("Already exists.  Skipping\n");
            exists = true;
            break;
        }
    }

    if (!exists)
    {
        _services.push_back(service);
        service->SetHandleManager(this->_handleManager);
        _handleManager->Assign(service);

        //  Instance method callback binding.
        service->OnCharacteristicAdded = std::bind(&BluetoothServiceCollection::HandleServiceCharacteristicAdded, this, std::placeholders::_1);

        if (OnServiceAdded != NULL)
        {
            OnServiceAdded(service);
        }
    }

    TRACE_MESSAGE("%i services known\n", (int)_services.size());

    return *service;
}

/**
 * @brief Process the addition of a new characteristic.
 * 
 * @param characteristic Characteristic being added.
 */
void BluetoothServiceCollection::HandleServiceCharacteristicAdded(BluetoothCharacteristic *characteristic)
{
    // pass the event upstream
    characteristic->OnValueChanged = std::bind(&BluetoothServiceCollection::HandleServiceValueChanged, this, std::placeholders::_1);

    if (OnCharacteristicAdded)
    {
        OnCharacteristicAdded(characteristic);
    }
}

/**
 * @brief Process the value changed event for a characteristic.
 * 
 * @param characteristic Characteristic whose value has changed.
 */
void BluetoothServiceCollection::HandleServiceValueChanged(BluetoothCharacteristic *characteristic)
{
    if (OnValueChanged)
    {
        OnValueChanged(characteristic);
    }
}

/**
 * @brief Find the service with the specified service UUID.
 * 
 * @param uuid UUID of the service to search for.
 * @return BluetoothService* Pointer to the service with the specified UUID, NULL if the service cannot be found.
 */
BluetoothService* BluetoothServiceCollection::Find(uint8_t *uuid)
{
    return Find(Uuid128(uuid));    
}

/**
 * @brief Find the service with the specified service UUID.
 * 
 * @param uuid UUID of the service to search for.
 * @return BluetoothService* Pointer to the service with the specified UUID, NULL if the service cannot be found.
 */
BluetoothService* BluetoothServiceCollection::Find(Uuid128 uuid)
{
    // do we already know about this service?  If so, ignore 
    bool exists = false;

    for (auto it = _services.begin() ; it != _services.end(); ++it)
    {
        auto svc = (BluetoothService *) *it;

        if (svc->Uuid == uuid)
        {
            TRACE_MESSAGE("Found service with matching UUID\n");
            return svc;
        }
    }

    return NULL;
}

/**
 * @brief Get a count of the number of services registered.
 * 
 * @return int Number of registered services.
 */
int BluetoothServiceCollection::Count()
{
    return _services.size();
}