#include "BluetoothServiceCollection.hpp"

BluetoothService& BluetoothServiceCollection::Add(BluetoothService *service)
{
    // do we already know about this service?  If so, ignore 
    bool exists = false;

    for (auto it = _services.begin() ; it != _services.end(); ++it) {
        auto svc = (BluetoothService*)*it;

        if(svc->Uuid == service->Uuid) {
            printf("Already exists.  Skipping\n");
            exists = true;
            break;
        }
    }

    if(!exists) {
        //printf("Adding service...\n");
        _services.push_back(service);
        service->SetHandleManager(this->_handleManager);
        _handleManager->Assign(service);

        // instance method callback binding.  Yay, c++!
        service->OnCharacteristicAdded = std::bind(&BluetoothServiceCollection::HandleServiceCharacteristicAdded, this, std::placeholders::_1);

        if(OnServiceAdded != NULL)
        {
            OnServiceAdded(service);
        }
    }

    printf("%i services known\n", _services.size());

    return *service;
}

void BluetoothServiceCollection::HandleServiceCharacteristicAdded(BluetoothCharacteristic *characteristic)
{
    // pass the event upstream
    if(OnCharacteristicAdded)
    {
        OnCharacteristicAdded(characteristic);
    }
}

BluetoothService* BluetoothServiceCollection::Find(uint8_t *uuid)
{
    return Find(Uuid128(uuid));    
}

BluetoothService* BluetoothServiceCollection::Find(Uuid128 uuid)
{
    // do we already know about this service?  If so, ignore 
    bool exists = false;

    for (auto it = _services.begin() ; it != _services.end(); ++it) {
        auto svc = (BluetoothService*)*it;

        if(svc->Uuid == uuid) {
            printf("Found service with matching UUID\n");
            return svc;
        }
    }

    return NULL;
}

int BluetoothServiceCollection::Count()
{
    return _services.size();
}