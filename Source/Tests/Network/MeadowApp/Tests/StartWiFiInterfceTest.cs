using System;
using System.Threading;
using System.Threading.Tasks;
using Meadow.Devices;

namespace MeadowApp
{
    public class StartWiFiInterfaceTest : IMeadowTest
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
        /// Indicate if the WiFiInterfaceStarted event fired.
        /// </summary>
        private bool _interfaceStartedEventFired = false;

        /// <summary>
        /// Indicate if the WiFiInterfaceStopped event fired.
        /// </summary>
        private bool _interfaceStoppedEventFired = false;

        /// <summary>
        /// Default constructor is private to prevent it being used.
        /// </summary>
        private StartWiFiInterfaceTest() { }

        /// <summary>
        /// Constructor for the test.
        /// </summary>
        /// <param name="device">Device object giving access to the underlying hardware.</param>
        /// <param name="messageDispatcher">Message dispatcher used to communicate with the host computer.</param>
        public StartWiFiInterfaceTest(F7Micro device, MessageDispatcher messageDispatcher)
        {
            _device = device;
            _messageDispatcher = messageDispatcher;
        }

        /// <summary>
        /// Name of the test being executed.
        /// </summary>
        public string Name()
        {
            return(nameof(StartWiFiInterfaceTest));
        }

        /// <summary>
        /// Setup the conditions for the test.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Setup()
        {
            _device.WiFiAdapter.WiFiInterfaceStarted += WiFiAdapter_WiFiInterfaceStarted;
            _device.WiFiAdapter.WiFiInterfaceStopped += WiFiAdapter_WiFiInterfaceStopped;
            return(true);
        }

        /// <summary>
        /// Record the fact that the WiFiInterfaceStarted event fired.
        /// </summary>
        private void WiFiAdapter_WiFiInterfaceStopped(object sender, System.EventArgs e)
        {
            lock(_lock)
            {
                _interfaceStoppedEventFired = true;
            }
        }

        /// <summary>
        /// Record the fact that the WiFiInterfaceStarted event fired.
        /// </summary>
        private void WiFiAdapter_WiFiInterfaceStarted(object sender, System.EventArgs e)
        {
            lock(_lock)
            {
                _interfaceStartedEventFired = true;
            }
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

            bool started = false;
            _device.WiFiAdapter.StartWiFiInterface();
            started = CheckEventVariable(ref _interfaceStartedEventFired, true, timeout);
            bool stopped = false;
            if (started)
            {
                _device.WiFiAdapter.StopWiFiInterface();
                CheckEventVariable(ref _interfaceStoppedEventFired, true, timeout);
                if (!stopped)
                {
                    _messageDispatcher.SendMessage($"LOG: Could not stop network interface within {timeout} seconds.");
                }
            }
            else
            {
                _messageDispatcher.SendMessage($"LOG: Could not start network interface within {timeout} seconds.");
            }
            return (started && stopped);
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
            _device.WiFiAdapter.WiFiInterfaceStarted -= WiFiAdapter_WiFiInterfaceStarted;
            _device.WiFiAdapter.WiFiInterfaceStopped -= WiFiAdapter_WiFiInterfaceStopped;
            return (true);
        }
    }
}
