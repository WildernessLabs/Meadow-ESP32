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
        static void HandleValueChanged(BluetoothCharacteristic *);
        static void HandleDescriptorAdded(BluetoothDescriptor *);

    public:
        BluetoothManager();
        BluetoothServiceCollection Services;

        // TODO: add high-level graph handler so consumer only uses this instance to manage all BT stuff
};

#endif