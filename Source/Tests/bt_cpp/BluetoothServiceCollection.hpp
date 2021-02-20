#ifndef _BLUETOOTH_SERVICE_COLLECTION_HPP_
#define _BLUETOOTH_SERVICE_COLLECTION_HPP_

#include <string>
#include <vector>

#include "Uuid.hpp"
#include "BluetoothService.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothHandleManager.hpp"

using namespace std;

class BluetoothServiceCollection
{
    private:
        vector<BluetoothService*> _services;
        BluetoothHandleManager* _handleManager;

        void HandleServiceCharacteristicAdded(BluetoothCharacteristic *);

    public:
        BluetoothServiceCollection(BluetoothHandleManager *handleManager)
        {
            _handleManager = handleManager;
        }

        ServiceAdded OnServiceAdded = NULL;
        CharacteristicAdded OnCharacteristicAdded = NULL;

        BluetoothService& Add(BluetoothService *service);
        BluetoothService* Find(uint8_t *uuid);
        BluetoothService* Find(Uuid128 uuid);
        int Count();
};

#endif