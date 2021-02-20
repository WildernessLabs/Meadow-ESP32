#include "BluetoothManager.hpp"

static uint8_t service_uuid1[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
};

static uint8_t service_uuid2[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x01,
};

static const uint16_t characteristic_declaration_uuid = 0x2803;

int main()
{
    // if you're reading this and wondering "why didn't he use a unit test framework?"
    // go take a look at the 50-steps required to use a C++ test framework in VS Code for Mac

    printf("Starting...\r\n");
    BluetoothManager manager;

//    BluetoothService service1("Service 1", service_uuid1);
    BluetoothService service2("Service 2", service_uuid2);

    manager
        .Services
        .Add(
            new BluetoothService("Service 1", service_uuid1)
        )
        .AddCharacteristic(
            new BluetoothCharacteristic(0x2a56, CharacteristicPermission::ReadEncrypted, CharacteristicProperty::Notify)
        ); 

    // service count should be 1
    auto count = manager.Services.Count();

    // try re-adding the same service - no action should happen
    manager
        .Services
        .Add(
            new BluetoothService("Service 1", service_uuid1)
        );

    // service count should STILL be 1
    count = manager.Services.Count();

    auto existing = manager.Services.Find(service_uuid1);


    // try re-adding the service, with the same characteristic - no action should happen
    manager.Services.Add(existing);

    // try re-adding the service, with a new characteristic
    existing->AddCharacteristic(
            new BluetoothCharacteristic(0x2a57, CharacteristicPermission::ReadEncrypted, CharacteristicProperty::Notify)
    );

//        .AddCharacteristic(
//            new BluetoothCharacteristic("Characteristic A", 0x2a56)
//        );

    manager.Services.Add(&service2);

//    BluetoothCharacteristic heartRate()

//    service2.AddCharacteristic(&heartRate);

    printf("Done.");
}