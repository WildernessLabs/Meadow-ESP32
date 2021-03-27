using System;
using System.IO;
using System.Reflection;
using System.Threading;
using Meadow;
using Meadow.Devices;

namespace MeadowApp
{
    class BluetoothStartupTest : App<F7Micro, BluetoothStartupTest>
    {
        public BluetoothStartupTest()
        {

        }

        public void Startup()
        {

            Device.InitBluetoothAdapter();

            var config = LoadConfigResource("config.json");

            while (Device.BluetoothAdapter == null)
            {
                Console.WriteLine("!!! BluetoothAdapter is NULL");
                Thread.Sleep(1000);
            }

            Device.BluetoothAdapter.StartBluetoothStack(config);
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