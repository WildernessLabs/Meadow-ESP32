using System;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    /// <summary>
    /// Verify that the Connect to access point method is working.
    /// 
    /// This is a synchronous test and it loops for (up to) 60 seconds to check
    /// for a network connection.
    /// </summary>
    public class DisconnectFromAccessPointEventsTest : IMeadowTest
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
        public bool AsyncTest { get { return (true); } }

        /// <summary>
        /// Indicate if the WiFiInterfaceStarted event fired.
        /// </summary>
        private bool _interfaceStartedEventFired = false;

        /// <summary>
        /// Indicate if the WiFiInterfaceStopped event fired.
        /// </summary>
        private bool _interfaceStoppedEventFired = false;

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
        private DisconnectFromAccessPointEventsTest() { }

        /// <summary>
        /// Constructor for the test.
        /// </summary>
        /// <param name="device">Device object giving access to the underlying hardware.</param>
        /// <param name="messageDispatcher">Message dispatcher used to communicate with the host computer.</param>
        public DisconnectFromAccessPointEventsTest(F7Micro device, MessageDispatcher messageDisaptcher)
        {
            _device = device;
            _messageDispatcher = messageDisaptcher;
        }

        /// <summary>
        /// Name of the test being executed.
        /// </summary>
        public string Name()
        {
            return(nameof(DisconnectFromAccessPointEventsTest));
        }

        /// <summary>
        /// Setup the conditions for the test.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Setup()
        {
            _device.WiFiAdapter.WiFiInterfaceStarted += WiFiAdapter_WiFiInterfaceStarted;
            _device.WiFiAdapter.WiFiInterfaceStopped += WiFiAdapter_WiFiInterfaceStopped;
            _device.WiFiAdapter.WiFiConnected += WiFiAdapter_WiFiConnected;
            _device.WiFiAdapter.WiFiDisconnected += WiFiAdapter_WiFiDisconnected;
            return(true);
        }

        /// <summary>
        /// Record the fact that the WiFiInterfaceStarted event fired.
        /// </summary>
        private void WiFiAdapter_WiFiInterfaceStopped(object sender, System.EventArgs e)
        {
            lock (_lock)
            {
                _interfaceStoppedEventFired = true;
            }
        }

        /// <summary>
        /// Record the fact that the WiFiInterfaceStarted event fired.
        /// </summary>
        private void WiFiAdapter_WiFiInterfaceStarted(object sender, System.EventArgs e)
        {
            lock (_lock)
            {
                _interfaceStartedEventFired = true;
            }
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
        /// <exception cref="NotImplementedException"></exception>
        public bool Execute()
        {
            throw new NotImplementedException(nameof(ExecuteAsync));
        }

        /// <summary>
        /// Execute the test asynchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public async Task<bool> ExecuteAsync()
        {
            int timeout = 90;

            Console.WriteLine($"Connecting to WiFi Network {Secrets.WIFI_NAME}");
            var connectionResult = await _device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot connect to network: {connectionResult.ConnectionStatus}");
            }

            bool started = false;
            bool connected = false;
            bool disconnected = false;
            bool stopped = false;
            if ((_device.WiFiAdapter.IsConnected) && (_device.WiFiAdapter.IpAddress != new IPAddress(0x00000000)))
            {
                started = CheckEventVariable(ref _interfaceStartedEventFired, true, timeout);
                if (started)
                {
                    connected = CheckEventVariable(ref _connectedEventFired, true, timeout);
                    if (connected)
                    {
                        Thread.Sleep(10000);
                        _device.WiFiAdapter.Disconnect(true).Wait();
                        disconnected = CheckEventVariable(ref _disconnectedEventFired, true, timeout);
                    }
                    if (disconnected)
                    {
                        stopped = CheckEventVariable(ref _interfaceStoppedEventFired, true, timeout);
                    }
                    else
                    {
                        _messageDispatcher.SendMessage($"LOG: Disconnect event not generated within {timeout} seconds.");
                    }
                    if (!stopped)
                    {
                        _messageDispatcher.SendMessage($"LOG: StopInterface event not generated within {timeout} seconds.");
                    }
                }
                else
                {
                    _messageDispatcher.SendMessage($"LOG: StartInterface event not generated within {timeout} seconds.");
                }
            }
            else
            {
                _messageDispatcher.SendMessage($"LOG: Connected event not generated within {timeout} seconds.");
            }

            return (started && connected && disconnected && stopped && (_device.WiFiAdapter.IsConnected == false));
        }

        /// <summary>
        /// Tear down the test restoring the system to a state where other tests can be run.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Teardown()
        {
            _device.WiFiAdapter.WiFiInterfaceStarted -= WiFiAdapter_WiFiInterfaceStarted;
            _device.WiFiAdapter.WiFiInterfaceStopped -= WiFiAdapter_WiFiInterfaceStopped;
            _device.WiFiAdapter.WiFiConnected += WiFiAdapter_WiFiConnected;
            _device.WiFiAdapter.WiFiDisconnected += WiFiAdapter_WiFiDisconnected;
            return (true);
        }
    }
}
