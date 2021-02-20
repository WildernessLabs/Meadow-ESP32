#ifndef _BLUETOOTH_MANAGER_HPP_
#define _BLUETOOTH_MANAGER_HPP_

#include "BluetoothServiceCollection.hpp"
#include "BluetoothAttribute.hpp"
#include "BluetoothHandleManager.hpp"
#include <vector>

using namespace std;

class BluetoothManager 
{
    private:
        BluetoothHandleManager _handleManager;

        static void HandleServiceAdded(BluetoothService *);
        static void HandleCharacteristicAdded(BluetoothCharacteristic *);

    public:
        BluetoothManager();
        BluetoothServiceCollection Services;
};

#endif