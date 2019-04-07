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

        public SPITestApplication()
        {
            _redLED = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDRed, true);
            _blueLED = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDBlue, true);
            _greenLED = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDGreen, true);
            _reset = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);
            _boot = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);

            _stm32ReadyPin = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_UART_RX, false);
            _esp32ReadyPin = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX);
        }

        /// <summary>
        /// Wait for the ESP32 to indicate that it is ready to process a command.
        ///
        /// This is indicated by a pulse on the ESP32 Ready pin.
        /// </summary>
        private void WaitForEsp32ToBeReady()
        {
            while (_esp32ReadyPin.State == false) ;
            while (_esp32ReadyPin.State) ;
        }

        public void Run()
        {
            Console.WriteLine();
            Console.WriteLine("Basic SPI Test Communication (0.10.04)");
            Console.WriteLine();
            Console.WriteLine("Resetting ESP32");

            _boot.State = true;
            _reset.State = false;
            _reset.State = true;

            //SoftwareSPIBus esp32 = new SoftwareSPIBus(Device, Device.Pins.D05, Device.Pins.D06, Device.Pins.D07, Device.Pins.D08);
            var esp32 = new SoftwareSPIBus(Device, Device.Pins.ESP_MOSI, Device.Pins.ESP_MISO, Device.Pins.ESP_CLK, Device.Pins.ESP_CS);

            UInt32 counter = 0;
            Messages.Message startMessage;
            startMessage.Interface = (byte) Esp32Interfaces.Transport;
            startMessage.MessageType = (byte) TransportPacketType.StartOfMessage;
            startMessage.Function = 0;
            startMessage.StatusCode = 0;
            startMessage.PayloadLength = 0;
            startMessage.Crc = 0;
            startMessage.Payload = null;

            while (true)
            {
                _blueLED.State = !_blueLED.State;

                startMessage.MessageID = counter;
                Console.WriteLine($"Sending messageID : '{counter}'");
                counter++;

                _stm32ReadyPin.State = false;
                _stm32ReadyPin.State = true;
                WaitForEsp32ToBeReady();
                Console.WriteLine("ESP32 is ready.");
                _stm32ReadyPin.State = false;

                //
                //  Send the message header to the ESP32.
                //
                byte[] buffer = Encoders.EncodeMessage(startMessage);
                string bufferText = DebugHelpers.PrintableBuffer(buffer);
                Console.WriteLine(bufferText);
                esp32.WriteBytes(buffer);

                _stm32ReadyPin.State = false;
                Thread.Sleep(500);
            }
        }
    }
}
