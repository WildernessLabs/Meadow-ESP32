using System;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;

namespace MeadowApp
{
    /// <summary>
    /// Provide a test for the method that retrieves the device information from the Meaddow board.
    /// </summary>
    public class DeviceInfoTest : IMeadowTest
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
        private DeviceInfoTest() { }

        /// <summary>
        /// Constructor for the test.
        /// </summary>
        /// <param name="device">Device object giving access to the underlying hardware.</param>
        /// <param name="messageDispatcher">Message dispatcher used to communicate with the host computer.</param>
        public DeviceInfoTest(F7Micro device, MessageDispatcher messageDispatcher)
        {
            _device = device;
            _messageDispatcher = messageDispatcher;
        }

        /// <summary>
        /// Name of the test being executed.
        /// </summary>
        public string Name()
        {
            return(nameof(DeviceInfoTest));
        }

        /// <summary>
        /// Set up the test scenario, in this case there is nothing to do.
        /// </summary>
        /// <returns>Always returns true.</returns>
        public bool Setup()
        {
            return(true);
        }

        /// <summary>
        /// Execute the test code to get information about the device.
        /// </summary>
        /// <returns>Always returns true.</returns>
        public bool Execute()
        {
            _messageDispatcher.SendMessage($"MachineName: {Environment.MachineName}");
            _messageDispatcher.SendMessage($"DeviceName: {_device.Information.DeviceName}");
            _messageDispatcher.SendMessage($"OsVersion: {MeadowOS.SystemInformation.OSVersion}");
            _messageDispatcher.SendMessage($"MicrocontrollerSerialNumber: {_device.Information.ProcessorSerialNumber}");
            _messageDispatcher.SendMessage($"MicrocontrollerId: {_device.Information.ChipID}");
            _messageDispatcher.SendMessage($"Model: {_device.Information.Model}");
            _messageDispatcher.SendMessage($"MonoVersion: {MeadowOS.SystemInformation.MonoVersion}");
            _messageDispatcher.SendMessage($"ProcessorType: {_device.Information.ProcessorType}");
            _messageDispatcher.SendMessage($"Product: {_device.Information.Model}");
            _messageDispatcher.SendMessage($"BuildDate: {MeadowOS.SystemInformation.OSBuildDate}");
            _messageDispatcher.SendMessage($"CoprocessorType: {_device.Information.CoprocessorType}");
            _messageDispatcher.SendMessage($"CoprocessorFirmwareVersion: {_device.Information.CoprocessorOSVersion}");
            _messageDispatcher.SendMessage($"MacAddress: {Utilities.MacAddressString(_device.WiFiAdapter.MacAddress)}");

            return(true);
        }

        /// <summary>
        /// This method is not used.
        /// </summary>
        /// <returns>Always returns true.</returns>
        /// <exception cref="NotImplementedException"></exception>
        public Task<bool> ExecuteAsync()
        {
            throw new NotImplementedException(nameof(ExecuteAsync));
        }

        /// <summary>
        /// Tear down the test scenario, in this case there is nothing to do.
        /// </summary>
        /// <returns>Always returns true.</returns>
        public bool Teardown()
        {
            return(true);
        }
    }
}
