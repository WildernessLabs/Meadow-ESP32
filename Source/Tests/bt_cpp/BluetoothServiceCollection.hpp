#ifndef _BLUETOOTH_SERVICE_COLLECTION_HPP_
#define _BLUETOOTH_SERVICE_COLLECTION_HPP_

#include <string>
#include <vector>

#include "Uuid.hpp"
#include "BluetoothCharacteristic.hpp"
#include "BluetoothService.hpp"

using namespace std;

class BluetoothServiceCollection
{
    private:
        vector<BluetoothService*> _services;

    public:
        BluetoothService& Add(BluetoothService *service);
        BluetoothService* Find(uint8_t *uuid);
        BluetoothService* Find(Uuid128 uuid);
        int Count();
};

#endif