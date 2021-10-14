using System.Collections.Generic;
using Meadow;
using Meadow.Devices;

namespace MeadowApp
{
    public partial class TestRunner : App<F7Micro, TestRunner>
    {
        /// <summary>
        /// Dictionary of all of the tests available.
        /// </summary>
        /// <typeparam name="string">Name of the test.</typeparam>
        /// <typeparam name="IMeadowTest">Class implementing the test.</typeparam>
        Dictionary<string, IMeadowTest> _tests = new Dictionary<string, IMeadowTest>();

        /// <summary>
        /// Populate the dictionary with all of the tests that can be run.
        /// </summary>
        private void PopulateTestDictionary()
        {
            _tests.Add(nameof(DeviceInfoTest), new DeviceInfoTest(Device, _messageDispatcher));
            _tests.Add(nameof(ConnectToAccessPointTest), new ConnectToAccessPointTest(Device, _messageDispatcher));
            _tests.Add(nameof(StartWiFiInterfaceTest), new StartWiFiInterfaceTest(Device, _messageDispatcher));
            _tests.Add(nameof(ConnectToAccessPointEventsTest), new ConnectToAccessPointEventsTest(Device, _messageDispatcher));
            _tests.Add(nameof(DisconnectFromAccessPointEventsTest), new DisconnectFromAccessPointEventsTest(Device, _messageDispatcher));
            _tests.Add(nameof(OauthTest), new OauthTest(Device, _messageDispatcher));
            _tests.Add(nameof(NtpTest), new NtpTest(Device, _messageDispatcher));
            _tests.Add(nameof(HttpRequestTest), new HttpRequestTest(Device, _messageDispatcher));
            _tests.Add(nameof(HttpsRequestTest), new HttpsRequestTest(Device, _messageDispatcher));
            _tests.Add(nameof(PiSoakTest10000), new PiSoakTest10000(Device, _messageDispatcher));
        }
    }
}
