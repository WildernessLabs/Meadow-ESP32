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

        public SPITestApplication()
        {
            _redLED = new DigitalOutputPort(Device.Pins.OnboardLEDRed, false);
            _blueLED = new DigitalOutputPort(Device.Pins.OnboardLEDBlue, false);
            _greenLED = new DigitalOutputPort(Device.Pins.OnboardLEDGreen, false);
            _reset = new DigitalOutputPort(Device.Pins.ESP_RST, true);
            _boot = new DigitalOutputPort(Device.Pins.ESP_BOOT, true);
        }

        public void Run()
        {
            _boot.State = true;
            _reset.State = false;
            _reset.State = true;

            Console.WriteLine();
            Console.WriteLine("Basic SPI Test Communication");
            Console.WriteLine();

            //SoftwareSPIBus esp32 = new SoftwareSPIBus(Device.Pins.D05, Device.Pins.D06, Device.Pins.D07, Device.Pins.D08);
            SoftwareSPIBus esp32 = new SoftwareSPIBus(Device.Pins.ESP_MOSI, Device.Pins.ESP_MISO, Device.Pins.ESP_CLK, Device.Pins.ESP_CS);

            UInt32 counter = 0;
            Messages.Message message;
            message.Interface = (byte) Esp32Interfaces.System;
            message.MessageType = (byte) TransportPacketType.Header;
            message.Function = (UInt32) SystemFunction.GetConfiguration;
            message.StatusCode = (UInt32) StatusCodes.CompletedOK;
            message.PayloadLength = 0;
            message.Crc = 0;
            message.Payload = null;

            while (true)
            {
                _blueLED.State = !_blueLED.State;

                message.MessageID = counter;
                Console.WriteLine($"Sending messageID : '{counter}'");
                counter++;

                byte[] buffer = Encoders.EncodeMessage(message);
                string bufferText = DebugHelpers.PrintableBuffer(buffer);
                Console.WriteLine(bufferText);
                esp32.WriteBytes(buffer);

                Thread.Sleep(500);
            }
        }
    }
}
