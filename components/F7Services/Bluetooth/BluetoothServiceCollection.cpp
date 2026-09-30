/**
 * @file BluetoothServiceCollection.cpp
 * @author Chris Tacke
 * @brief Implementation of the BluetoothServiceCollection class.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#include "Logging.hpp"

#include "BluetoothServiceCollection.hpp"
#include "BluetoothComponentName.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
const char *BluetoothServiceCollection::COMPONENT_NAME = BLUETOOTH_COMPONENT_NAME;

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Add a new service to the services collection.
 *
 * @param service Service to be added.
 * @return BluetoothService&
 */
BluetoothService &BluetoothServiceCollection::Add(BluetoothService * const service)
{
    TRACE_MESSAGE("Add: Enter");

    // Do we already know about this service?  If so, ignore
    bool exists = false;

    for (auto it = _services.begin(); it != _services.end(); ++it)
    {
        auto svc = static_cast<BluetoothService *>(*it);

        if (svc->Uuid == service->Uuid)
        {
            TRACE_MESSAGE("Already exists.  Skipping");
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

        if (OnServiceAdded != nullptr)
        {
            OnServiceAdded(service);
        }
    }

    TRACE_MESSAGE("%" PRIu32 " services known", static_cast<uint32_t>(_services.size()));

    TRACE_MESSAGE("Add: Exit");

    return *service;
}

/**
 * @brief Delete all of the registered services.
 *
 * @note This assumes that all of the services have already been stopped.
 *
 * @return bool If successfully deleted, false otherwise.
 */
bool BluetoothServiceCollection::DeleteAllServices()
{
    TRACE_MESSAGE("DeleteAllServices: Enter");

    for (auto it = _services.begin(); it != _services.end(); ++it)
    {
        auto service = static_cast<BluetoothService *>(*it);
        delete service;
    }

    _services.clear();

    TRACE_MESSAGE("DeleteAllServices: Exit");

    return true;
}

/**
 * @brief Get the service definitions in this collection.
 *
 * @return vector<BluetoothService *> Service definitions.
 */
std::vector<BluetoothService *> BluetoothServiceCollection::GetServiceDefinitions()
{
    return _services;
}

/**
 * @brief Handle the service characteristic added event.
 *
 * @param characteristic Pointer to the characteristic that has been added to the service.
 */
void BluetoothServiceCollection::HandleServiceCharacteristicAdded(BluetoothCharacteristic * const characteristic)
{
    TRACE_MESSAGE("HandleServiceCharacteristicAdded: Enter");

    // Pass the event upstream
    characteristic->OnValueChanged = std::bind(&BluetoothServiceCollection::HandleServiceValueChanged, this, std::placeholders::_1);

    if (OnCharacteristicAdded)
    {
        OnCharacteristicAdded(characteristic);
    }

    TRACE_MESSAGE("HandleServiceCharacteristicAdded: Exit");
}

/**
 * @brief Handle the service value changed event.
 *
 * @param characteristic Characteristic that has had its value changed.
 */
void BluetoothServiceCollection::HandleServiceValueChanged(BluetoothCharacteristic * const characteristic)
{
    TRACE_MESSAGE("HandleServiceValueChanged: Enter");

    if (OnValueChanged)
    {
        OnValueChanged(characteristic);
    }

    TRACE_MESSAGE("HandleServiceValueChanged: Exit");
}

/**
 * @brief Find a service in the collection by UUID.
 *
 * @param uuid Pointer to an array of bytes containing the 16-bit UUID fro the service.
 * @return BluetoothService* Pointer to the service if found, nullptr otherwise.
 */
BluetoothService *BluetoothServiceCollection::Find(const uint8_t * const uuid)
{
    return Find(Uuid16(uuid));
}

/**
 * @brief Find a service in the collection by UUID.
 *
 * @param uuid Uuid16 object containing the UUID of the service to be found.
 * @return BluetoothService* Pointer to the service if found, nullptr otherwise.
 */
BluetoothService *BluetoothServiceCollection::Find(const Uuid16 uuid)
{
    TRACE_MESSAGE("Find: Enter");

    for (auto it = _services.begin(); it != _services.end(); ++it)
    {
        auto svc = static_cast<BluetoothService *>(*it);

        if (svc->Uuid == uuid)
        {
            TRACE_MESSAGE("Found service with matching UUID");
            return svc;
        }
    }

    TRACE_MESSAGE("Find: Exit");

    return nullptr;
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