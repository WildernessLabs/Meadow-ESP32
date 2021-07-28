using System;
using System.Threading;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Foundation;
using Meadow.Gateway.WiFi;
using Meadow.Gateways;

namespace MeadowApp
{
    public class WiFiInterfaceControl : App<F7Micro, WiFiInterfaceControl>
    {
        public WiFiInterfaceControl()
        {
            if (!Device.InitWiFiAdapter().Result)
            {
                throw new Exception("Could not initialise the WiFi adapter.");
            }
        }

        public void RunTest()
        {
            Device.WiFiAdapter.WiFiInterfaceStarted += WiFiAdapter_WiFiInterfaceStarted;
            Device.WiFiAdapter.WiFiInterfaceStopped += WiFiAdapter_WiFiInterfaceStopped;
            Device.WiFiAdapter.WiFiConnected += WiFiAdapter_WiFiConnected;

            Console.WriteLine("Starting WiFi interface.");
            if (Device.WiFiAdapter.StartWiFiInterface())
            {
                Console.WriteLine("WiFi interface started.");
                if (Device.WiFiAdapter.StopWiFiInterface())
                {
                    Console.WriteLine("WiFi interface stopped.");
                }
                else
                {
                    Console.WriteLine("WiFi interface could not be stopped.");
                }
            }
            else
            {
                Console.WriteLine("WiFi interface could not be started.");
            }
        }

        public async Task RunAsyncTest()
        {
            Console.WriteLine("Connecting to access point.");

            Device.WiFiAdapter.WiFiInterfaceStarted += WiFiAdapter_WiFiInterfaceStarted;
            Device.WiFiAdapter.WiFiInterfaceStopped += WiFiAdapter_WiFiInterfaceStopped;
            Device.WiFiAdapter.WiFiConnected += WiFiAdapter_WiFiConnected;

            var connectionResult = await Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            Console.WriteLine($"Connect to access point result: {connectionResult.ConnectionStatus}");
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot connect to network: {connectionResult.ConnectionStatus}");
            }
        }

        private void WiFiAdapter_WiFiConnected(object sender, EventArgs e)
        {
            WiFiConnectEventArgs ea = (WiFiConnectEventArgs) e;
            Console.WriteLine($"WiFiConnected event raised at {ea.When.ToString("HH:mm:ss")}, status code: {ea.StatusCode}");
            Console.WriteLine($"    IP Address: {ea.IpAddress}");
            Console.WriteLine($"    Subnet mask: {ea.Subnet}");
            Console.WriteLine($"    Gateway: {ea.Gateway}");
            Console.WriteLine($"    SSID: {ea.Ssid}");
            Console.WriteLine($"    BSSID: {ea.Bssid}");
            Console.WriteLine($"    Channel: {ea.Channel}");
            Console.WriteLine($"    Authentication type: {ea.AuthenticationType}");
        }

        private void WiFiAdapter_WiFiInterfaceStopped(object sender, EventArgs e)
        {
            WiFiInterfaceStoppedEventArgs ea = (WiFiInterfaceStoppedEventArgs) e;
            Console.WriteLine($"WiFiInterfaceStopped event raised at {ea.When.ToString("HH:mm:ss")}, status code: {ea.StatusCode}");
        }

        private void WiFiAdapter_WiFiInterfaceStarted(object sender, EventArgs e)
        {
            WiFiInterfaceStartedEventArgs ea = (WiFiInterfaceStartedEventArgs) e;
            Console.WriteLine($"WiFiInterfaceStarted event raised at {ea.When.ToString("HH:mm:ss")}, status code: {ea.StatusCode}");
        }
    }
}
