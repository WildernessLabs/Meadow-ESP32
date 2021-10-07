using System;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Foundation;
using Meadow.Foundation.Leds;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class DeviceInfo : App<F7Micro, DeviceInfo>
    {
        private string MacAddressString(byte[] address)
        {
            string result = String.Empty;

            for (int index = 0; index < address.Length; index++)
            {
                result += address[index].ToString("X2");
                if (index != (address.Length - 1))
                {
                    result += ":";
                }
            }
            return (result);
        }

        public DeviceInfo()
        {
        }

        public void RunTest()
        {
            Console.WriteLine("Getting device information.");

            Console.WriteLine($"Machine name: {Environment.MachineName}");

            F7Micro.DeviceInformation information = Device.GetDeviceInformation();

            Console.WriteLine($"Device name: {information.DeviceName}");
            Console.WriteLine($"OS version: {information.OsVersion}");
            Console.WriteLine($"Microcontroller serial number: {information.SerialNumber}");
            Console.WriteLine($"Microcontroller ID: {information.UniqueId}");
            Console.WriteLine($"Model: {information.Model}");
            Console.WriteLine($"Mono version: {information.MonoVersion}");
            Console.WriteLine($"Processor type: {information.ProcessorType}");
            Console.WriteLine($"Product: {information.Product}");
            Console.WriteLine($"Build date: {information.BuildDate}");
            Console.WriteLine($"Coprocessor type: {information.CoprocessorType}");
            Console.WriteLine($"Coprocessor firmware version: {information.CoprocessorFirmwareVersion}");
            //
            //  Get some of the OS configuration values.
            //
            // bool autmaticallStartNetwork = Device.WiFiAdapter.AutomaticallyStartNetwork;
            // Console.WriteLine($"Automatically connect to network: {autmaticallStartNetwork}");

            // bool automaticallyReconnect = Device.WiFiAdapter.AutomaticallyReconnect;
            // Console.WriteLine($"Automatically reconnect: {automaticallyReconnect}");

            // bool getTimeAtStartup = Device.WiFiAdapter.GetNetworkTimeAtStartup;
            // Console.WriteLine($"Get time at startup: {getTimeAtStartup}");

            // Console.WriteLine($"Default access point: {Device.WiFiAdapter.DefaultAcessPoint}");

            // uint maximumRetryCount = Device.WiFiAdapter.MaximumRetryCount;
            // Console.WriteLine($"Maximum retry count: {maximumRetryCount}");

            // Console.WriteLine($"MAC address: {MacAddressString(Device.WiFiAdapter.MacAddress)}");
            // Console.WriteLine($"Soft AP MAC address: {MacAddressString(Device.WiFiAdapter.ApMacAddress)}");
            //
            //  Try to change some values.
            //
            // Console.WriteLine("Setting AutomaticallyReconnect");
            // Device.WiFiAdapter.SetAutomaticallyReconnect(!automaticallyReconnect);

            // Console.WriteLine("Setting AutomaticallyStartNetwork");
            // Device.WiFiAdapter.SetAutomaticallyStartNetowrk(!autmaticallStartNetwork);

            // Console.WriteLine("Setting MaximumRetryCount");
            // maximumRetryCount++;
            // Device.WiFiAdapter.SetMaximumRetryCount(maximumRetryCount);

            // Console.WriteLine("Setting GetTimeAtStartUp");
            // Device.WiFiAdapter.SetGetNetworkTimeAtStartup(!getTimeAtStartup);

            // Console.WriteLine("Setting DeviceName");
            // Device.SetDeviceNmae("TestDevice");
            //
            //  Now get the changed values.
            //
            // Console.WriteLine($"Automatically connect to network: {Device.WiFiAdapter.AutomaticallyStartNetwork}");
            // Console.WriteLine($"Automatically reconnect: {Device.WiFiAdapter.AutomaticallyReconnect}");
            // Console.WriteLine($"Get time at startup: {Device.WiFiAdapter.GetNetworkTimeAtStartup}");
            // Console.WriteLine($"Maximum retry count: {Device.WiFiAdapter.MaximumRetryCount}");
        }
    }
}
