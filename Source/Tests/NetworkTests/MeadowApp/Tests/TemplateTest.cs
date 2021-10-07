using System;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class TemplateTest : IMeadowTest
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
        /// Default constructor is private to prevent it being used.
        /// </summary>
        private TemplateTest() { }

        /// <summary>
        /// Constructor for the test.
        /// </summary>
        /// <param name="device">Device object giving access to the underlying hardware.</param>
        /// <param name="messageDispatcher">Message dispatcher used to communicate with the host computer.</param>
        public TemplateTest(F7Micro device, MessageDispatcher messageDispatcher)
        {
            _device = device;
            _messageDispatcher = messageDispatcher;
        }

        /// <summary>
        /// Name of the test being executed.
        /// </summary>
        public string Name()
        {
            return(nameof(TemplateTest));
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
            return(true);
        }

        /// <summary>
        /// Execute the test asynchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        /// <exception cref="NotImplementedException"></exception>
        public async Task<bool> ExecuteAsync()
        {
            throw new NotImplementedException(nameof(TemplateTest));
            //bool result = true;
            //Console.WriteLine($"Connecting to WiFi Network {Secrets.WIFI_NAME}");
            //var connectionResult = await _device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            //if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            //{
            //    throw new Exception($"Cannot connect to network: {connectionResult.ConnectionStatus}");
            //}
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
