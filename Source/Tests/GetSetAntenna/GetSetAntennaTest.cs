using System;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Core.Esp32;

namespace BasicNetworkTest
{
    public class GetSetAntennaTest : App<F7Micro, GetSetAntennaTest>
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
        private Coprocessor _coprocessor;
        
        #endregion Private member variables (fields)
        
        #region Constructors
        
        /// <summary>
        /// Default constructor.
        /// </summary>
        public GetSetAntennaTest()
        {
            //
            //  The UART pins need instantiating as inputs in order to allow the ESP32 debug information to
            //  appear on any serial console attached to the ESP32 chip.
            //
            _esp32UartRx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_RX, resistorMode: ResistorMode.Disabled);
            _esp32UartTx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX, resistorMode: ResistorMode.Disabled);

            MessageDispatcher dispatcher = new MessageDispatcher(Device,
                mosi: Device.Pins.ESP_MOSI, miso: Device.Pins.ESP_MISO, clock: Device.Pins.ESP_CLK, chipSelect: Device.Pins.ESP_CS,
                bootPin: Device.Pins.ESP_BOOT, resetPin: Device.Pins.ESP_RST);

            _coprocessor = new Coprocessor(dispatcher);
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
        /// Display the name of an antenna.
        /// </summary>
        /// <param name="antenna">AntennaTypes object representing the antenna.</param>
        private void DisplayAntennaInformation(AntennaTypes antenna)
        {
            switch (antenna)
            {
                case AntennaTypes.External:
                    DebugHelpers.DebugHelpers.LogMessage(true, "External antenna is in use.");
                    break;
                case AntennaTypes.OnBoard:
                    DebugHelpers.DebugHelpers.LogMessage(true, "On board antenna is in use.");
                    break;
                default:
                    DebugHelpers.DebugHelpers.LogMessage(true, $"Unknown antenna {(byte) antenna} is in use.");
                    break;
            }
        }

        /// <summary>
        /// Run the GetAntenna / SetAntenna tests.
        /// </summary>
        private void RunTest()
        {
            //
            //  Get the  current antenna, should be the on board antenna at start up.
            //
            DebugHelpers.DebugHelpers.LogMessage(true, "Requesting antenna information.");
            AntennaTypes antenna = _coprocessor.GetAntenna();
            DisplayAntennaInformation(antenna);
            //
            //  Change the antenna to the external antenna.
            //
            DebugHelpers.DebugHelpers.LogMessage(true, "Changing antenna in use to the external antenna");
            antenna = AntennaTypes.External;
            _coprocessor.SetAntenna(antenna);
            //
            //  Now get the antenna information again to make sure it has changed to the external antenna.
            //
            DebugHelpers.DebugHelpers.LogMessage(true, "Requesting new antenna information.");
            antenna = _coprocessor.GetAntenna();
            DisplayAntennaInformation(antenna);
        }
        
        #endregion Methods
    }
}
