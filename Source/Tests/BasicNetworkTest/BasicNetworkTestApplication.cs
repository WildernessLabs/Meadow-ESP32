using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Hardware.Communications;
using Meadow.Core.Esp32;
using System.Threading;

namespace BasicNetworkTest
{
    public class BasicNetworkTestApplication : AppBase<F7Micro, BasicNetworkTestApplication>
    {
        private DigitalInputPort _button;

        MessageDispatcher _dispatcher = null;

        /// <summary>
        /// Default constructor.
        /// </summary>
        public BasicNetworkTestApplication()
        {
            _dispatcher = new MessageDispatcher(Device, Device.Pins.ESP_UART_RX, Device.Pins.ESP_UART_TX, Device.Pins.ESP_MOSI, Device.Pins.ESP_MISO, Device.Pins.ESP_CLK, Device.Pins.ESP_CS, Device.Pins.ESP_BOOT, Device.Pins.ESP_RST);
            _dispatcher.MessageReceived += _dispatcher_MessageReceived;
            _dispatcher.Error += _dispatcher_Error;
            _button = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.D15, interruptMode: InterruptMode.EdgeFalling, resistorMode: ResistorMode.PullUp, debounceDuration: 100);
            _button.Changed += _button_Changed;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _button_Changed(object sender, DigitalInputPortEventArgs e)
        {
            Messages.AntennaInfo antennaInfo = new Messages.AntennaInfo();
            antennaInfo.Antenna = (byte) Antenna.External;
            Messages.Message message = new Messages.Message();
            message.Interface = (byte) Esp32Interfaces.WiFi;
            message.Function = (byte)WiFiFunction.GetAntenna;
            message.MessageID = 0x54;
            message.MessageType = (byte)PacketType.Data;
            _dispatcher.Add(message);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _dispatcher_Error(object sender, MessageDispatcherErrorEventArgs e)
        {
            Console.WriteLine($"Dispatcher error: {e.StatusCode}");
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _dispatcher_MessageReceived(object sender, MessageReceviedEventArgs e)
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Main programe loop.
        /// </summary>
        public void Run()
        {
            Console.WriteLine("Basic Network Communication Test.");
            Console.WriteLine();
            Console.WriteLine("Press button to start the test.");
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
