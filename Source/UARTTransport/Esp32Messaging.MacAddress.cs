using System;
using System.Collections.Generic;
using System.Text;

namespace Esp32Messaging
{
    public class MacAddress
    {
        public byte[] Address { get; private set; }

        private MacAddress() { }

        public MacAddress(byte[] address)
        {
            if (address.Length != 6)
            {
                throw new ArgumentException(nameof(address), "MAC address should be 6 bytes long.");
            }
            Address = address;
        }

        public override string ToString()
        {
            string result = "";
            for (int index = 0; index < Address.Length; index++)
            {
                result += Address[index].ToString("X2");
                if (index < (Address.Length - 1))
                {
                    result += ":";
                }
            }
            return (result);
        }
    }
}
