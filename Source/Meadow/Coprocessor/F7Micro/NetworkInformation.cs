namespace Meadow.Core.Esp32
{
    /// <summary>
    /// Information about a network. 
    /// </summary>
    public class NetworkInformation
    {
        /// <summary>
        /// Four bytes of an IP4 IP address.
        /// </summary>
        public byte[] ip4IpAddress = {0x00, 0x00, 0x00, 0x00};
    }
}
