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
    /// <summary>
    /// Create a HttpClient object and repeatedly request web pages from a local server.
    /// </summary>
    /// <remarks>
    /// 
    /// The aim of this test is to test the long term stability of the network stack.
    /// 
    /// </remarks>
    public class DisconnectTest : App<F7Micro, DisconnectTest>
    {
        public DisconnectTest()
        {
            Console.WriteLine("Initialising WiFi Adapter object.");

            Device.InitWiFiAdapter().Wait();
        }
        
        public async Task RunTest()
        {
            Console.WriteLine($"Connecting to WiFi Network {Secrets.WIFI_NAME}");
            var connectionResult = await Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot connect to network: {connectionResult.ConnectionStatus}");
            }
            else
            {
                Console.WriteLine("Network connection successful.");
            }

            connectionResult = await Device.WiFiAdapter.Disconnect(true);
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot disconnect from network: {connectionResult.ConnectionStatus}");
            }
            else
            {
                Console.WriteLine("Network disconnection successful.");
            }

            Thread.Sleep(2000);

            Console.WriteLine("Attempting second disconnect.");
            connectionResult = await Device.WiFiAdapter.Disconnect(true);
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot disconnect from network: {connectionResult.ConnectionStatus}");
            }
            else
            {
                Console.WriteLine("Network disconnection successful.");
            }
        }
    }
}
