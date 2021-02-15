#ifndef _BLUETOOTH_MANAGER_HPP_
#define _BLUETOOTH_MANAGER_HPP_

#include "BluetoothAttribute.hpp"
#include "BluetoothService.hpp"
#include <vector>

using namespace std;

class BluetoothManager {

    private:
        vector<BluetoothService*> _services;

    public:
        BluetoothService& AddService(BluetoothService *service);
        BluetoothService* FindService(uint8_t *uuid);
        BluetoothService* FindService(Uuid128 uuid);
};

#endif