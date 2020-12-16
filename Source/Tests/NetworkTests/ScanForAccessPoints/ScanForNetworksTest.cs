using System;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace BasicNetworkTest
{
    public class ScanForNetworksTest : App<F7Micro, ScanForNetworksTest>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public ScanForNetworksTest()
        {
            Console.WriteLine("Connecting to network.");
            Device.InitWiFiAdapter().Wait();
        }

        public void Log(string message)
        {
            string output = DateTime.Now.ToString("HH:mm:ss");
            output += ": ";
            output += message;
            Console.WriteLine(output);
        }

        public void RunTest()
        {
            int count = 0;
            
            while (true)
            {
                count++;
                Log($"Executing scan {count}");
                Log("Getting list of access points.");
                Device.WiFiAdapter.Scan();
                if (Device.WiFiAdapter.Networks.Count > 0)
                {
                    Log("|-------------------------------------------------------------|---------|");
                    Log("|         Network Name             | RSSI |       BSSID       | Channel |");
                    Log("|-------------------------------------------------------------|---------|");
                    foreach (WifiNetwork accessPoint in Device.WiFiAdapter.Networks)
                    {
                        Log($"| {accessPoint.Ssid,-32} | {accessPoint.SignalDbStrength,4} | {accessPoint.Bssid,17} |   {accessPoint.ChannelCenterFrequency,3}   |");
                    }
                }
                else
                {
                    Log($"No access points detected.");
                }
            }
        }
    }
}
