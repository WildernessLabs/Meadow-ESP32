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
    public class DnsTest : App<F7Micro, DnsTest>
    {
        private void LookupAddress(string host)
        {
            try
            {
                Console.WriteLine($"Looking for {host}");
                IPHostEntry ipe = Dns.GetHostEntry(host);
                Console.WriteLine("Host found.");
                foreach (IPAddress address in ipe.AddressList)
                {
                    Console.WriteLine($"    Address: {address.ToString()}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{ex.Message}");
            }
        }

        public async Task RunAsync()
        {
            Console.WriteLine($"Connecting to WiFi Network {Secrets.WIFI_NAME}");
            var connectionResult = await Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot connect to network: {connectionResult.ConnectionStatus}");
            }

            LookupAddress("az220-training-hub.azure-devices.net");
            LookupAddress("nevyn-hub.azure-devices.net");
            LookupAddress("g2cert.azure-devices.net");
        }
    }
}