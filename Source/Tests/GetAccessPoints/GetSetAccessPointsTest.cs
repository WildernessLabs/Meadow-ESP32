using System;
using System.Collections.Generic;
using System.Net;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Coprocessor.F7Micro;
using Meadow.Gateway.WiFi;

namespace BasicNetworkTest
{
    public class GetSetAccessPointsTest : App<F7Micro, GetSetAccessPointsTest>
    {
        #region Private member variables (fields)
        
        /// <summary>
        /// Button that can be used to trigger a test.
        /// </summary>
        private DigitalInputPort _button;
        
        /// <summary>
        /// ESP32 UART Rx port.
        /// </summary>
        IDigitalInputPort _esp32UartRx;
        
        /// <summary>
        /// ESP32 UART Tx port.
        /// </summary>
        IDigitalInputPort _esp32UartTx;

        /// <summary>
        /// Network coprocessor.
        /// </summary>
        private readonly F7MicroCoprocessor _f7MicroCoprocessor;
        
        /// <summary>
        /// Message dispatcher responsible for sending messages to the ESP32 and also passing
        /// any messages from the ESP32 to the relevant interface on the STM32.
        /// </summary>
        private readonly MessageDispatcher _dispatcher;
        
        #endregion Private member variables (fields)
        
        #region Constructors
        
        /// <summary>
        /// Default constructor.
        /// </summary>
        public GetSetAccessPointsTest()
        {
            //
            //  The UART pins need instantiating as inputs in order to allow the ESP32 debug information to
            //  appear on any serial console attached to the ESP32 chip.
            //
            _esp32UartRx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_RX, resistorMode: ResistorMode.Disabled);
            _esp32UartTx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX, resistorMode: ResistorMode.Disabled);

            _dispatcher = new MessageDispatcher(Device,
                mosi: Device.Pins.ESP_MOSI, miso: Device.Pins.ESP_MISO, clock: Device.Pins.ESP_CLK, chipSelect: Device.Pins.ESP_CS,
                bootPin: Device.Pins.ESP_BOOT, resetPin: Device.Pins.ESP_RST);

            _f7MicroCoprocessor = new F7MicroCoprocessor(_dispatcher);
            //
            //  Wire up the button to run the tests on demand.
            //
            _button = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.D14,
                interruptMode: InterruptMode.EdgeFalling, resistorMode: ResistorMode.PullUp, debounceDuration: 500);
            _button.Changed += _button_Changed;
            //
            //  Now run the test once.  Further tests can be run by pressing the button on D14.
            //
            RunTest();
        }
        
        #endregion Constructors
        
        #region Events
        
                
        /// <summary>
        /// Run the test when the button is pressed.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _button_Changed(object sender, DigitalInputPortEventArgs e)
        {
            RunTest();
        }

        #endregion Events
        
        #region Methods

        /// <summary>
        /// Run the GetAccessPoints test.
        /// </summary>
        private void RunTest()
        {
            DebugHelpers.DebugHelpers.LogMessage(true, "Starting network"); 
            
            IPAddress ipAddress = _f7MicroCoprocessor.StartNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSOWRD);
            DebugHelpers.DebugHelpers.LogMessage(true, $"Obtained IP address: {ipAddress.ToString()}");

            DebugHelpers.DebugHelpers.LogMessage(true, "Getting list of access points.");
            List<WifiNetwork> accessPoints = _f7MicroCoprocessor.GetAccessPoints();
            if (accessPoints != null)
            {
                DebugHelpers.DebugHelpers.LogMessage(true, "|-------------------------------------------------------------|---------|");
                DebugHelpers.DebugHelpers.LogMessage(true, "|         Network Name             | RSSI |       BSSID       | Channel |");
                DebugHelpers.DebugHelpers.LogMessage(true, "|-------------------------------------------------------------|---------|");
                foreach (WifiNetwork accessPoint in accessPoints)
                {
                    DebugHelpers.DebugHelpers.LogMessage(true, $"| {accessPoint.Ssid,-32} | {accessPoint.SignalDbStrength,4} | {accessPoint.Bssid,17} |   {accessPoint.ChannelCenterFrequency,3}   |");
                }
            }
            else
            {
                DebugHelpers.DebugHelpers.LogMessage(true, $"No access points detected.");
            }
        }
        
        #endregion Methods
    }
}
