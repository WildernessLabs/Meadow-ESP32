using System;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace BasicNetworkTest
{
    public class ConfigurationItemsTest : App<F7Micro, ConfigurationItemsTest>
    {
        #region Constructors
        
        /// <summary>
        /// Default constructor.
        /// </summary>
        public ConfigurationItemsTest()
        {
            Device.InitWiFiAdapter().Wait();
            Console.WriteLine("Connecting to network.");
            if (Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception("Cannot connect to network.");
            }
        }

        #endregion Constructors

        #region Methods

        /// <summary>
        /// Convert a MAC address into a series of bytes separated by colns.
        /// </summary>
        /// <param name="macAddress">MAC address to convert.</param>
        /// <returns>String representation of the MAC address.</returns>
        private string MacToString(byte[] macAddress)
        {
            StringBuilder address = new StringBuilder();
            bool firstByte = true;

            foreach (byte b in macAddress)
            {
                if (!firstByte)
                {
                    address.Append(":");
                }
                firstByte = false;
                address.Append(b.ToString("X02"));
            }

            return (address.ToString());
        }

        /// <summary>
        /// Run the ConfigurationItems test.
        /// </summary>
        public void RunTest()
        {
            Console.WriteLine("Getting ESP32 Configuration.");
            Console.WriteLine($"Device name: {Device.WiFiAdapter.DeviceName}");
            Console.WriteLine($"Get network time at startup: {Device.WiFiAdapter.GetNetworkTimeAtStartup}");
            Console.WriteLine($"NTP Server: {Device.WiFiAdapter.NtpServer}");
            Console.WriteLine($"Default access point: {Device.WiFiAdapter.DefaultAcessPoint}");
            Console.WriteLine($"Automatically start network: {Device.WiFiAdapter.AutomaticallyStartNetwork}");
            Console.WriteLine($"Automatically reconnect on error: {Device.WiFiAdapter.AutomaticallyReconnect}");
            Console.WriteLine($"MAC address: {MacToString(Device.WiFiAdapter.MacAddress)}");
            Console.WriteLine($"Soft AP MAC address: {MacToString(Device.WiFiAdapter.ApMacAddress)}");
            Console.WriteLine($"IP address: {Device.WiFiAdapter.IpAddress.ToString()}");
            Console.WriteLine($"Subnet mask: {Device.WiFiAdapter.SubnetMask.ToString()}");
            Console.WriteLine($"Default gateway: {Device.WiFiAdapter.Gateway.ToString()}");
        }

        #endregion Methods
    }
}
