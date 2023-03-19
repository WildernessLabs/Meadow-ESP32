using System;
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
    public class ConnectToAccessPointTest : IMeadowTest
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
        /// Default constructor is private to prevent it being used.
        /// </summary>
        private ConnectToAccessPointTest() { }

        /// <summary>
        /// Constructor for the test.
        /// </summary>
        /// <param name="device">Device object giving access to the underlying hardware.</param>
        /// <param name="messageDispatcher">Message dispatcher used to communicate with the host computer.</param>
        public ConnectToAccessPointTest(F7Micro device, MessageDispatcher messageDisaptcher)
        {
            _device = device;
            _messageDispatcher = messageDisaptcher;
        }

        /// <summary>
        /// Name of the test being executed.
        /// </summary>
        public string Name()
        {
            return(nameof(ConnectToAccessPointTest));
        }

        /// <summary>
        /// Setup the conditions for the test.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Setup()
        {
            return(true);
        }

        /// <summary>
        /// Execute the test synchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Execute()
        {
            bool result = true;

            _messageDispatcher.SendMessage($"Connecting to network {Secrets.WIFI_NAME}");
            _device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            int count = 0;
            while ((!_device.WiFiAdapter.IsConnected) && (count < 60))
            {
                count++;
                Thread.Sleep(1000);
            }
            if (!_device.WiFiAdapter.IsConnected)
            {
                _messageDispatcher.SendMessage("LOG: Could not connect to WiFi within 60 seconds.");
                result = false;
            }
            
            return(result);
        }

        /// <summary>
        /// Execute the test asynchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
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
            return(true);
        }
    }
}
