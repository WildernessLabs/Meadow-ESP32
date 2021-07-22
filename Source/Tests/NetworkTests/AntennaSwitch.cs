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
    public class AntennaSwitch : App<F7Micro, AntennaSwitch>
    {
        public AntennaSwitch()
        {
            if (!Device.InitWiFiAdapter().Result)
            {
                throw new Exception("Could not initialize the WiFi adapter.");
            }
        }

        public void RunTest()
        {
            // Make sure the on board antenna is selected.
            Device.SetAntenna(AntennaType.OnBoard);
            Console.WriteLine($"Current antenna in use: {Device.CurrentAntenna}");
            ScanForAccessPoints();

            Console.WriteLine($"Switching to external antenna.");
            Device.SetAntenna(AntennaType.External);
            Console.WriteLine($"Current antenna in use: {Device.CurrentAntenna}");

            ScanForAccessPoints();

            Console.WriteLine($"Switching back to on board antenna.");
            Device.SetAntenna(AntennaType.OnBoard);
            Console.WriteLine($"Current antenna in use: {Device.CurrentAntenna}");
        }

        private void ScanForAccessPoints()
        {
            Console.WriteLine("Getting list of access points.");
            var networks = Device.WiFiAdapter.Scan();
            if (networks.Count > 0)
            {
                Console.WriteLine("|-------------------------------------------------------------|---------|");
                Console.WriteLine("|         Network Name             | RSSI |       BSSID       | Channel |");
                Console.WriteLine("|-------------------------------------------------------------|---------|");
                foreach (WifiNetwork accessPoint in networks)
                {
                    Console.WriteLine($"| {accessPoint.Ssid,-32} | {accessPoint.SignalDbStrength,4} | {accessPoint.Bssid,17} |   {accessPoint.ChannelCenterFrequency,3}   |");
                }
            } 
            else
            {
                Console.WriteLine($"No access points detected.");
            }
        }
    }
}
