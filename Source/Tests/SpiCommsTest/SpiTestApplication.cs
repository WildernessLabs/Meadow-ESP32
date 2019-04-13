//define USE_MEADOW

using System;
using System.Text;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Hardware.Communications;
using Meadow.Core.Esp32;


namespace SPITest
{
    public class SPITestApplication : AppBase<F7Micro, SPITestApplication>
    {
        private DigitalOutputPort _redLED;
        private DigitalOutputPort _blueLED;
        private DigitalOutputPort _greenLED;
        private DigitalOutputPort _reset;
        private DigitalOutputPort _boot;
        private DigitalOutputPort _stm32ReadyPin;
        private DigitalInputPort _esp32ReadyPin;
        private DigitalInputPort _button;
        private UInt32 _counter = 0;
        Messages.Message _startMessage;
        SoftwareSPIBus _esp32;


        public SPITestApplication()
        {
            _redLED = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDRed, true);
            _blueLED = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDBlue, true);
            _greenLED = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDGreen, true);



#if (USE_MEADOW)
            _reset = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);
            _boot = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);

            _stm32ReadyPin = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_UART_RX, false);
            _esp32ReadyPin = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX);
#else
            _stm32ReadyPin = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.D05, false);
            _esp32ReadyPin = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.D06);
#endif
            _button = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.D15, interruptMode: InterruptMode.EdgeFalling, resistorMode: ResistorMode.PullUp);
            _button.Changed += _button_Changed;
        }

        private void _button_Changed(object sender, DigitalInputPortEventArgs e)
        {
            _blueLED.State = !_blueLED.State;
            _startMessage.MessageID = _counter;
            Console.WriteLine($"Sending messageID : '{_counter}'");
            _counter++;

            Console.WriteLine("    Waiting for ESP32 to be ready.");
            _stm32ReadyPin.State = true;
            WaitForEsp32ToBeReady();
            Console.WriteLine("    ESP32 is ready.");

            Console.WriteLine();
            Console.WriteLine("Sending message:");
            //
            //  Send the message header to the ESP32.
            //
            byte[] buffer = Encoders.EncodeMessage(_startMessage);
            string bufferText = DebugHelpers.PrintableBuffer(buffer);
            Console.WriteLine(bufferText);
            _esp32.WriteBytes(buffer);

            Console.WriteLine("    Message sent.");
            Console.WriteLine();
            Console.WriteLine("Waiting for button press.");

            _stm32ReadyPin.State = false;
            _blueLED.State = !_blueLED.State;
        }

        /// <summary>
        /// Wait for the ESP32 to indicate that it is ready to process a command.
        ///
        /// This is indicated by a pulse on the ESP32 Ready pin.
        /// </summary>
        private void WaitForEsp32ToBeReady()
        {
            while (_esp32ReadyPin.State == false) ;
            //while (_esp32ReadyPin.State) ;
        }

        public void Run()
        {
            _startMessage = new Messages.Message();

#if (USE_MEADOW)
            _esp32 = new SoftwareSPIBus(Device, Device.Pins.ESP_MOSI, Device.Pins.ESP_MISO, Device.Pins.ESP_CLK, Device.Pins.ESP_CS);

            _boot.State = true;
            _reset.State = false;
            _reset.State = true;
#else
            _esp32 = new SoftwareSPIBus(Device, Device.Pins.D07, Device.Pins.D08, Device.Pins.D09, Device.Pins.D10);
#endif

            Console.WriteLine();
            Console.WriteLine("Basic SPI Test Communication (0.10.06)");
            Console.WriteLine();
            Console.WriteLine("Resetting ESP32");

            _stm32ReadyPin.State = false;

            Console.WriteLine();
            Console.WriteLine("Waiting for button press.");

            _startMessage.Interface = (byte) Esp32Interfaces.Transport;
            _startMessage.MessageType = (byte) TransportPacketType.StartOfMessage;
            _startMessage.Function = 0;
            _startMessage.StatusCode = 0;
            _startMessage.PayloadLength = 0;
            _startMessage.Crc = 0;
            _startMessage.Payload = null;

            Thread.Sleep(Timeout.Infinite);
        }
    }
}
