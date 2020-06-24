using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Gateway.WiFi;
using System.Net.Http;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

namespace BasicNetworkTest
{
    public class NetworkTest : App<F7Micro, NetworkTest>
    {
        static readonly HttpClient client = new HttpClient();

        /// <summary>
        /// Default constructor.
        /// </summary>
        public NetworkTest()
        {
            WifiNetwork wifi = new WifiNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD,NetworkType.Any, PhyType.Unknown,
                new NetworkSecuritySettings(NetworkAuthenticationType.Wpa2Psk, NetworkEncryptionType.None),
                6, NetworkProtocol.Protocol11G, -76);

            wifi.StartNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
        }

        public async Task GetWebPageAsync(string uri)
        {
            HttpResponseMessage response = await client.GetAsync(uri);

            response.EnsureSuccessStatusCode();
            string responseBody = await response.Content.ReadAsStringAsync();
            Console.WriteLine(responseBody);
        }
    }
}
