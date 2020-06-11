using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Gateway.WiFi;

namespace BasicNetworkTest
{
    public class NetworkTest : App<F7Micro, NetworkTest>
    {
        #region Private member variables (fields)

        #endregion Private member variables (fields)

        #region Constructors

        /// <summary>
        /// Default constructor.
        /// </summary>
        public NetworkTest()
        {
            WifiNetwork wifi = new WifiNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD,NetworkType.Any, PhyType.Unknown,
                new NetworkSecuritySettings(NetworkAuthenticationType.Wpa2Psk, NetworkEncryptionType.None),
                6, NetworkProtocol.Protocol11G, -76);

            wifi.StartNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
        }

        #endregion Constructors

    }
}
