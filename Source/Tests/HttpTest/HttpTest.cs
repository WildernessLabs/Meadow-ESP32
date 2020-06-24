using System;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Gateway.WiFi;
using System.Net.Http;
using System.Threading.Tasks;

namespace HttpTest
{
    public class HttpTest : App<F7Micro, HttpTest>
    {
        static readonly HttpClient client = new HttpClient();

        /// <summary>
        /// Default constructor.
        /// </summary>
        public HttpTest()
        {
            WifiNetwork wifi = new WifiNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD, NetworkType.Any, PhyType.Unknown,
                new NetworkSecuritySettings(NetworkAuthenticationType.Wpa2Psk, NetworkEncryptionType.None),
                6, NetworkProtocol.Protocol11G, -76);

            wifi.StartNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
        }

        public async Task GetWebPageAsync(string uri)
        {
            try
            {
                HttpResponseMessage response = await client.GetAsync(uri);

                response.EnsureSuccessStatusCode();
                string responseBody = await response.Content.ReadAsStringAsync();
                Console.WriteLine(responseBody);
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error: {e.Message}");
                throw;
            }
        }
    }
}