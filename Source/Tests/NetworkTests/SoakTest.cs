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
    public class SoakTest : App<F7Micro, SoakTest>
    {
        public SoakTest()
        {
            Console.WriteLine("Initialising WiFi Adapter object.");

            Device.InitWiFiAdapter().Wait();
        }
        
        public async void RunTest()
        {
            Console.WriteLine($"Connecting to WiFi Network {Secrets.WIFI_NAME}");
            var connectionResult = await Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot connect to network: {connectionResult.ConnectionStatus}");
            }

            using (HttpClient client = new HttpClient())
            {
                int count = 0;
                client.Timeout = new TimeSpan(0, 1, 0);
                while (true)
                {
                    count++;
                    Console.WriteLine($"Current request: {count}");
                    HttpResponseMessage response = await client.GetAsync($"http://{Secrets.SOAK_TEST_IP_ADDRESS}");
                    try
                    {
                        response.EnsureSuccessStatusCode();
                        string responseBody = await response.Content.ReadAsStringAsync();
                        responseBody = await response.Content.ReadAsStringAsync();
                        Console.WriteLine(responseBody);
                        Console.WriteLine("----------------------------------------------------------------------");
                    }
                    catch (TaskCanceledException)
                    {
                        Console.WriteLine("Request time out.");
                    }
                    catch (Exception e)
                    {
                        Console.WriteLine($"Request failed: {e.Message}");
                    }
                }
            }
        }
    }
}
