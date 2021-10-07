using System;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using Meadow.Devices;

namespace MeadowApp
{
    /// <summary>
    /// Verify that the Connect to access point method is working.
    /// 
    /// This is a synchronous test and it loops for (up to) 60 seconds to check
    /// for a network connection.
    /// </summary>
    public class ConnectToAccessPointEventsTest : IMeadowTest
    {
        /// <summary>
        /// F7Micro device object to allow access to the hardware.
        /// </summary>
        F7Micro _device;

        /// <summary>
        /// Message dispatcher used to exchange messages between the test system and the host computer.
        /// </summary>
        MessageDispatcher _messageDispatcher;

        /// <summary>
        /// Lock variable for the system state variables.
        /// </summary>
        private object _lock = new object();

        /// <summary>
        /// Indicate that this is an asynchronous test.
        ///
        /// The value of the test indicates which Execute method should be run:
        ///     true - ExecuteAsync should be called
        ///     false - Execute should be called.
        ///
        /// The invalid method should throw a NotImplementedException.
        /// </summary>
        public bool AsyncTest { get { return (false); } }

        /// <summary>
        /// Indicate if the WiFiConnected event fired.
        /// </summary>
        private bool _connectedEventFired = false;

        /// <summary>
        /// Indicate if the WiFiDisconnected event fired.
        /// </summary>
        private bool _disconnectedEventFired = false;

        /// <summary>
        /// Default constructor is private to prevent it being used.
        /// </summary>
        private ConnectToAccessPointEventsTest() { }

        /// <summary>
        /// Constructor for the test.
        /// </summary>
        /// <param name="device">Device object giving access to the underlying hardware.</param>
        /// <param name="messageDispatcher">Message dispatcher used to communicate with the host computer.</param>
        public ConnectToAccessPointEventsTest(F7Micro device, MessageDispatcher messageDisaptcher)
        {
            _device = device;
            _messageDispatcher = messageDisaptcher;
        }

        /// <summary>
        /// Name of the test being executed.
        /// </summary>
        public string Name()
        {
            return(nameof(ConnectToAccessPointEventsTest));
        }

        /// <summary>
        /// Setup the conditions for the test.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Setup()
        {
            _device.WiFiAdapter.WiFiConnected += WiFiAdapter_WiFiConnected;
            _device.WiFiAdapter.WiFiDisconnected += WiFiAdapter_WiFiDisconnected;
            return(true);
        }

        /// <summary>
        /// This event is generated when the application disconnects from a WiFi network.
        /// </summary>
        private void WiFiAdapter_WiFiDisconnected(object sender, EventArgs e)
        {
            lock(_lock)
            {
                _disconnectedEventFired = true;
            }
        }

        /// <summary>
        /// This event is generated when the the application connects to a WiFi network.
        /// </summary>
        private void WiFiAdapter_WiFiConnected(object sender, EventArgs e)
        {
            _connectedEventFired = true;
        }

        /// <summary>
        /// Check a variable to see if it changes during the timeout period.
        ///
        /// This can be used to check if an event changes a variable to an expected value.
        /// </summary>
        /// <param name="variable">Variable (reference) to be examined.</param>
        /// <param name="expectedValue">Value the varaible should change to.</param>
        /// <param name="timeout">Number of seconds to allow for the change.</param>
        /// <returns>true if the variable changes to the expected within the timeout period, flase otherwise.</returns>
        private bool CheckEventVariable(ref bool variable, bool expectedValue, int timeout)
        {
            int count;
            bool checking;
            bool result = !expectedValue;

            checking = true;
            count = 0;
            while (checking)
            {
                lock (_lock)
                {
                    result = variable;
                }
                checking = (count < timeout) && (result != expectedValue);
                if (checking)
                {
                    count++;
                    Thread.Sleep(1000);
                }
            }
            return (result);
        }

        /// <summary>
        /// Execute the test synchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Execute()
        {
            int timeout = 60;
            int count;

            _device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            count = 0;
            while ((!_device.WiFiAdapter.IsConnected) && (count < 60))
            {
                count++;
                Thread.Sleep(1000);
            }
            bool connected = false;
            bool disconnected = false;
            if ((_device.WiFiAdapter.IsConnected) && (_device.WiFiAdapter.IpAddress != new IPAddress(0x00000000)))
            {
                connected = CheckEventVariable(ref _connectedEventFired, true, timeout);
                if (connected)
                {
                    _device.WiFiAdapter.Disconnect(false).Wait();
                    disconnected = CheckEventVariable(ref _disconnectedEventFired, true, timeout);
                }
                if (!disconnected)
                {
                    _messageDispatcher.SendMessage($"LOG: Could not disconnect from WiFi within {timeout} seconds.");
                }
            }
            else
            {
                _messageDispatcher.SendMessage($"LOG: Could not connect to WiFi within {timeout} seconds.");
            }

            return (connected && disconnected);
        }

        /// <summary>
        /// Execute the test asynchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        /// <exception cref="NotImplementedException"></exception>
        public Task<bool> ExecuteAsync()
        {
            throw new NotImplementedException(nameof(ExecuteAsync));
        }

        /// <summary>
        /// Tear down the test restoring the system to a state where other tests can be run.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Teardown()
        {
            _device.WiFiAdapter.WiFiConnected += WiFiAdapter_WiFiConnected;
            _device.WiFiAdapter.WiFiDisconnected += WiFiAdapter_WiFiDisconnected;
            return (true);
        }
    }
}
