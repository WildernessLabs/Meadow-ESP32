using System;
using System.Net;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;
using System.Threading;
using System.Threading.Tasks;
using System.Net.Http;
using Meadow.Gateways;
using System.Collections.Generic;

namespace MeadowApp
{
    public class NtpTest : App<F7Micro, NtpTest>
    {

        public NtpTest()
        {
        }
        public async Task RunAsync()
        {
            Console.WriteLine($"Connecting to WiFi Network {Secrets.WIFI_NAME}");
            var connectionResult = await Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot connect to network: {connectionResult.ConnectionStatus}");
            }

            int counter = 0;
            string time = String.Empty;
            while ((time == String.Empty) || time.StartsWith("01 January 1970"))
            {
                Thread.Sleep(5000);
                counter++;
                time = DateTime.Now.ToString("dd MMMM yyyy HH:mm:ss");
                Console.WriteLine($"Attempt {counter}, current time: {time}");
            }
        }
    }
}