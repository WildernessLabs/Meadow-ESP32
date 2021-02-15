#include "BluetoothManager.hpp"

BluetoothService& BluetoothManager::AddService(BluetoothService *service)
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
        printf("Adding service...\n");
        // TODO: make a callback so this can be added to the stack
        _services.push_back(service);
    }

    printf("%i services known\n", _services.size());

    return *service;
}

BluetoothService* BluetoothManager::FindService(uint8_t *uuid)
{
    return FindService(Uuid128(uuid));    
}

BluetoothService* BluetoothManager::FindService(Uuid128 uuid)
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