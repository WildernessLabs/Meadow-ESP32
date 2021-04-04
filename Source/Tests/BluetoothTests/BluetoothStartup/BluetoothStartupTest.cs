using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Gateways.Bluetooth;

namespace MeadowApp
{
    class BluetoothStartupTest : App<F7Micro, BluetoothStartupTest>
    {
        private Definition _definition;

        public bool LoadFromFile { get; set; }

        public BluetoothStartupTest()
        {
            LoadFromFile = false;
        }

        public void Startup()
        {

            Device.InitBluetoothAdapter();

            while (Device.BluetoothAdapter == null)
            {
                Console.WriteLine("!!! BluetoothAdapter is NULL");
                Thread.Sleep(1000);
            }

            _definition = new Definition(
                "WTF MEADOW",
                new Service(
                    "ServiceA",
                    253,
                    new CharacteristicBool(
                        "My LED",
                        uuid: "017e99d6-8a61-11eb-8dcd-0242ac1300aa",
                        permissions: CharacteristicPermission.Write | CharacteristicPermission.Read,
                        properties: CharacteristicProperty.Write | CharacteristicProperty.Read,
                        readFunction: () =>
                        {
                            Console.WriteLine("Read LED has been called");
                            return new byte[] { 1, 2, 3, 4 };
                        },
                        descriptors:
                            new Descriptor(
                                uuid: "00002901-0000-1000-8000-00805f9b34fb", // well-known BT description
                                value: 22
                                )
                        ),
                    new CharacteristicInt32(
                        "My Number",
                        uuid: "017e99d6-8a61-11eb-8dcd-0242ac1300bb",
                        permissions: CharacteristicPermission.Write | CharacteristicPermission.Read,
                        properties: CharacteristicProperty.Write | CharacteristicProperty.Read
                        ),
                    new CharacteristicString(
                        "My Text",
                        uuid: "017e99d6-8a61-11eb-8dcd-0242ac1300cc",
                        maxLength: 20,
                        permissions: CharacteristicPermission.Write | CharacteristicPermission.Read,
                        properties: CharacteristicProperty.Write | CharacteristicProperty.Read
                        )
                    )
                );

            foreach (var c in _definition.Services[0].Characteristics)
            {
                c.ValueSet += (s, d) =>
                {
                    Console.WriteLine($"HEY, I JUST GOT THIS BLE DATA of type {d.GetType().Name}: {d}");
                };
            }

            Console.WriteLine("Starting BT stack");
            Device.BluetoothAdapter.StartBluetoothStack(_definition);            
        }

        private string LoadConfigResource(string filename)
        {
            var assembly = Assembly.GetExecutingAssembly();
            var resourceName = $"BluetoothStartup.{filename}";

            foreach(var name in assembly.GetManifestResourceNames())
            {
                Console.WriteLine($"resource: {name}");
            }

            using (var stream = assembly.GetManifestResourceStream(resourceName))
            using (var reader = new StreamReader(stream))
            {
                return reader.ReadToEnd();
            }
        }

        private const string config =
            @"{
              ""deviceName"": ""MEADOW BLE""
              ""services"": [
                {
                  ""name"": ""MyService"",
                  ""uuid"": 254,
                  ""characteristics"": [
                    {
                      ""uuid"": ""017e99d6-8a61-11eb-8dcd-0242ac130004"",
                      ""permission"": 17,
                      ""props"": 10,
                      ""len"": 4,
                      ""descriptors"": [
                        {
                          ""uuid"": 10498,
                          ""permission"": 9,
                          ""len"": 2,
                          ""value"": 00
                        }
                      ]
                    },
                    {
                ""uuid"": ""017e99d6-8a61-11eb-8dcd-0242ac130005"",
                      ""permission"": 2,
                      ""props"": 16,
                      ""len"": 4
                    }
                  ]
                }
              ]
            }";
    }
}