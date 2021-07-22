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
        public DeviceInfo()
        {
        }

        public void RunTest()
        {
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
            Console.WriteLine($"Coprocessor firmware version: {information.CoprocessorFirmwareVersion}");
            Console.WriteLine($"Coprocessor firmware type: {information.CoprocessorType}");
        }
    }
}
