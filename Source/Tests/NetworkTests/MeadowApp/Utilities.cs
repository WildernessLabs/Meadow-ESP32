using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Meadow;
using Meadow.Hardware;
using Meadow.Devices;
using Meadow.Foundation;
using Meadow.Foundation.Leds;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class Utilities
    {
        public static string MacAddressString(byte[] address)
        {
            string result = String.Empty;

            for (int index = 0; index < address.Length; index++)
            {
                result += address[index].ToString("X2");
                if (index != (address.Length - 1))
                {
                    result += ":";
                }
            }
            return (result);
        }
    }
}
