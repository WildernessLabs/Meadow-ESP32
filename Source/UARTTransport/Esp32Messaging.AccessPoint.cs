using System;
using System.Collections.Generic;
using System.Text;

namespace Esp32Messaging
{
    public class AccessPoint
    {
        public MacAddress Bssid { get; set; }

        public string Ssid { get; set; }

        public byte PrimaryChannel { get; set; }

        public WiFiSecondChannel SecondaryChannel { get; set; }

        public sbyte Rssi { get; set; }

        public WiFiAuthenticationMode AuthenticationMode { get; set; }

        public AccessPoint() { }
    }
}
