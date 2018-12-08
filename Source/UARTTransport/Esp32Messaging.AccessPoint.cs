namespace Esp32Messaging
{
    /// <summary>
    ///     Describe an access point.
    /// </summary>
    public class AccessPoint
    {
        #region Properties

        /// <summary>
        ///     Basic Service Set ID (MAC address) for the access point.
        /// </summary>
        public MacAddress Bssid { get; set; }

        /// <summary>
        ///     Service Set Identifier.
        /// </summary>
        public string Ssid { get; set; }

        /// <summary>
        ///     Primary channel the access point is operting on.
        /// </summary>
        public byte PrimaryChannel { get; set; }

        /// <summary>
        ///     Location of the secondary channel relative to the primary channel.
        /// </summary>
        public WiFiSecondChannel SecondaryChannel { get; set; }

        /// <summary>
        ///     Received Service Strength Indication.
        /// </summary>
        public sbyte Rssi { get; set; }

        /// <summary>
        ///     Method used to authenticate with the access point.
        /// </summary>
        public WiFiAuthenticationMode AuthenticationMode { get; set; }

        #endregion Properties

        #region Constructor(s)

        /// <summary>
        ///     Default constructor.
        /// </summary>
        public AccessPoint() { }

        #endregion Constructor(s)
    }
}
