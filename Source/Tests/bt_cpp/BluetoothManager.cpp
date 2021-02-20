#include "BluetoothManager.hpp"

BluetoothManager::BluetoothManager()
    : Services(&this->_handleManager)
{
    Services.OnServiceAdded = &BluetoothManager::HandleServiceAdded;
    Services.OnCharacteristicAdded = &BluetoothManager::HandleCharacteristicAdded;
}

void BluetoothManager::HandleServiceAdded(BluetoothService *service)
{
    printf("New Service: 0x%04X (%s)\n", service->Handle,  service->Name.c_str());
}

void BluetoothManager::HandleCharacteristicAdded(BluetoothCharacteristic *characteristic)
{
    printf("New Characteristic: declaration @ 0x%04X value @ 0x%04X\n", characteristic->Declaration->Handle,  characteristic->Value->Handle);
}
