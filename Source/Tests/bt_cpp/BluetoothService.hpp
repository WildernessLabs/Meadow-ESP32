#ifndef _BLUETOOTH_SERVICE_HPP_
#define _BLUETOOTH_SERVICE_HPP_

#include <string>
#include <vector>

#include "Uuid.hpp"
#include "BluetoothCharacteristic.hpp"

using namespace std;

class BluetoothService
{
    public:
        BluetoothService();
        BluetoothService(std::string name, Uuid128 uuid);
        BluetoothService(char* name, Uuid128 uuid);
        BluetoothService(char* name, uint8_t* uuid);
        BluetoothCharacteristic& AddCharacteristic(BluetoothCharacteristic *characteristic);

        std::string Name;
        Uuid128 Uuid;

    private:
        vector<BluetoothCharacteristic*> _characteristics;
};

#endif