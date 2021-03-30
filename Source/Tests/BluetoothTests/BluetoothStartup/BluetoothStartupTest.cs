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

            if (LoadFromFile)
            {
                var config = LoadConfigResource("config.json");

                Console.WriteLine("Starting BT stack");
                // no longer supported.  We need objects for events.
//                Device.BluetoothAdapter.StartBluetoothStack(config);
            }
            else
            {
                _definition = new Definition(
                    "MY MEADOW",
                    new Service(
                        name: "ServiceA",
                        uuid: 253,
                        characteristics:
                            new Characteristic(
                                "My LED",
                                uuid: "017e99d6-8a61-11eb-8dcd-0242ac1300aa",
                                permissions: CharacteristicPermission.Write | CharacteristicPermission.Read,
                                properties: CharacteristicProperty.Write | CharacteristicProperty.Read,
                                maxLength: 4
                                )
                            )
                    );

                _definition.Services[0].Characteristics[0].OnDataSet += (s, d) =>
                {
                    Console.WriteLine($"HEY, I JUST GOT THIS BLE DATA: {BitConverter.ToString(d)}");
                };

                Console.WriteLine("Starting BT stack");
                Device.BluetoothAdapter.StartBluetoothStack(_definition);
            }

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
                          ""permission"": 1,
                          ""len"": 2,
                          ""value"": 0
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