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
        private DigitalInputPort _button;
        
        IDigitalInputPort _esp32UartRx;
        
        IDigitalInputPort _esp32UartTx;

        private Coprocessor _coprocessor;
        
        public GetSetAntennaTest()
        {
            
            _esp32UartRx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_RX, resistorMode: ResistorMode.Disabled);
            _esp32UartTx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX, resistorMode: ResistorMode.Disabled);

            MessageDispatcher dispatcher = new MessageDispatcher(Device,
                mosi: Device.Pins.ESP_MOSI, miso: Device.Pins.ESP_MISO, clock: Device.Pins.ESP_CLK, chipSelect: Device.Pins.ESP_CS,
                bootPin: Device.Pins.ESP_BOOT, resetPin: Device.Pins.ESP_RST);
            
            _coprocessor = new Coprocessor(dispatcher);

            AntennaTypes antenna = _coprocessor.GetAntenna();
            switch (antenna)
            {
                case AntennaTypes.External:
                    Console.WriteLine("External antenna is in use.");
                    break;
                case AntennaTypes.OnBoard:
                    Console.WriteLine("On board antenna is in use.");
                    break;
                default:
                    Console.WriteLine($"Unknown antenna {(byte) antenna} is in use.");
                    break;
            }
        }
        
        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _button_Changed(object sender, DigitalInputPortEventArgs e)
        {
        }
    }
}
