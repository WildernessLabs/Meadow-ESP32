#include "BluetoothService.hpp"

BluetoothService::BluetoothService()
{
}

BluetoothService::BluetoothService(std::string name, Uuid128 uuid)
{
    Name.assign(name);
    Uuid = uuid;
}

BluetoothService::BluetoothService(char* name, Uuid128 uuid)
{
    Name.assign(name);
    Uuid = uuid;
}

BluetoothService::BluetoothService(char* name, uint8_t* uuid)
{
    Name.assign(name);
    Uuid = uuid;
}

BluetoothCharacteristic& BluetoothService::AddCharacteristic(BluetoothCharacteristic *characteristic)
{
    // do we already know about this characteristic?  If so, ignore 
    bool exists = false;

    if(characteristic->Declaration->Handle == 0)
    {
        _handleManager->Assign(characteristic->Declaration);
    }
    if(characteristic->Value->Handle == 0)
    {
        _handleManager->Assign(characteristic->Value);
    }

    for (auto it = _characteristics.begin() ; it != _characteristics.end(); ++it) {
        auto chr = (BluetoothCharacteristic*)*it;

        if(chr->Declaration->Handle == characteristic->Declaration->Handle) {
            printf("Already exists.  Skipping\n");
            exists = true;
            break;
        }
    }

    if(!exists) {
        _characteristics.push_back(characteristic);

        if(OnCharacteristicAdded)
        {
            OnCharacteristicAdded(characteristic);
        }
    }

    printf("%i characteristics known\n", _characteristics.size());

    return *characteristic;
}
