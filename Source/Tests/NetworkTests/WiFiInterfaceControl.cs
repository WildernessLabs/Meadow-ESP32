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
