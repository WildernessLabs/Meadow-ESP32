#define USE_MEADOW

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
        byte[] _dummyTransportPacket;
        SoftwareSPIBus _esp32;


        public SPITestApplication()
        {
            _redLED = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDRed, true);
            _blueLED = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDBlue, true);
            _greenLED = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.OnboardLEDGreen, true);



#if (USE_MEADOW)
            _reset = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);
            _boot = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);

            _stm32ReadyPin = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_UART_RX, false);
            _esp32ReadyPin = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX);
#else
            _stm32ReadyPin = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.D05, false);
            _esp32ReadyPin = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.D06);
#endif
            _button = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.D15, interruptMode: InterruptMode.EdgeFalling, resistorMode: ResistorMode.PullUp, debounceDuration: 100);
            _button.Changed += _button_Changed;
        }

        private void SendMessage()
        {
            _blueLED.State = !_blueLED.State;
            _counter++;
            _startMessage.MessageID = _counter;
            Console.WriteLine($"Sending messageID : '{_counter}'");


            Console.WriteLine("    Waiting for ESP32 to be ready.");
            _stm32ReadyPin.State = true;
            WaitForEsp32ToBeReady();
            Console.WriteLine("    ESP32 is ready.");
            string bufferText;
            byte[] receiveBuffer;

            //
            //  Now send the Antenna switch message.
            //
            Console.WriteLine();Console.WriteLine("Sending switch antenna message.");
            Messages.AntennaInfo switchAntenna = new Messages.AntennaInfo();
            switchAntenna.Antenna = (byte) Antenna.OnBoard;
            byte[] encodedSwitchAntenna = Encoders.EncodeAntennaInfo(switchAntenna);

            Messages.Message switchAntennaMessage = new Messages.Message();

            switchAntennaMessage.Interface = (byte) Esp32Interfaces.WiFi;
            switchAntennaMessage.Function = (UInt32) WiFiFunction.SetAntenna;
            switchAntennaMessage.MessageID = _counter;
            switchAntennaMessage.Payload = encodedSwitchAntenna;
            switchAntennaMessage.PayloadLength = (UInt32) encodedSwitchAntenna.Length;
            //
            //  Send switch Antenna header first.
            //
            switchAntennaMessage.MessageType = (byte) (PacketType.Header);
            byte[] encodedSwitchAntennaHeader = Encoders.EncodeHeader(switchAntennaMessage);
            bufferText = DebugHelpers.PrintableBuffer(encodedSwitchAntennaHeader);
            Console.WriteLine("Sending switch antenna header:");
            Console.WriteLine(bufferText);
            _esp32.WriteBytes(encodedSwitchAntennaHeader);
            _stm32ReadyPin.State = false;
            //
            //  Now we need an ACK.
            //
            Console.WriteLine("Sending request for header ACK / NAK.");
            Console.WriteLine();
            WaitForEsp32ToBeReady();
            _stm32ReadyPin.State = true;
            bufferText = DebugHelpers.PrintableBuffer(_dummyTransportPacket);
            Console.WriteLine(bufferText);
            receiveBuffer = _esp32.WriteRead(_dummyTransportPacket, (ushort)_dummyTransportPacket.Length);
            Console.WriteLine("Received:");
            bufferText = DebugHelpers.PrintableBuffer(receiveBuffer);
            Console.WriteLine(bufferText);
            _stm32ReadyPin.State = false;
            //
            //  Assuming an ACK, we can send the actual message.
            //
            Console.WriteLine("Sending the switch antenna message.");
            Console.WriteLine();
            WaitForEsp32ToBeReady();
            _stm32ReadyPin.State = true;
            switchAntennaMessage.MessageType = (byte) (PacketType.Data);
            byte[] encodedSwitchAntennaMessage = Encoders.EncodeMessage(switchAntennaMessage);
            bufferText = DebugHelpers.PrintableBuffer(encodedSwitchAntennaMessage);
            Console.WriteLine(bufferText);
            _esp32.WriteBytes(encodedSwitchAntennaMessage);
            _stm32ReadyPin.State = false;
            //
            //  Now give the ESP32 chance to send the ACK/NAK packet.
            //
            Console.WriteLine("Sending request for message body ACK / NAK.");
            Console.WriteLine();
            WaitForEsp32ToBeReady();
            _stm32ReadyPin.State = true;
            bufferText = DebugHelpers.PrintableBuffer(_dummyTransportPacket);
            Console.WriteLine(bufferText);
            receiveBuffer = _esp32.WriteRead(_dummyTransportPacket, (ushort)_dummyTransportPacket.Length);
            Console.WriteLine("Received:");
            bufferText = DebugHelpers.PrintableBuffer(receiveBuffer);
            Console.WriteLine(bufferText);

            Console.WriteLine();
            Console.WriteLine("Waiting for button press.");

            _stm32ReadyPin.State = false;
            _blueLED.State = !_blueLED.State;
        }

        private void DisplayMessage(Messages.Message message)
        {
            Console.WriteLine("Message:");
            Console.WriteLine($"    Message type: {DebugHelpers.Hexadecimal(message.MessageType)}");
            Console.WriteLine($"    Interface: {DebugHelpers.Hexadecimal(message.Interface)}");
            Console.WriteLine($"    ID: {message.MessageID}");
            Console.WriteLine($"    Function: {message.Function}");
            Console.WriteLine($"    Payload length: {message.PayloadLength}");
        }

        private void ReceiveMessage()
        {
            byte[] encodedPacketFromEsp32;
            byte[] encodedAckMessage;
            Messages.Message ackMessage = new Messages.Message();
            Messages.Message readyMessage = new Messages.Message();

            //
            //  Set this up once, it will be used multiple times.
            //
            ackMessage.MessageType = (byte) PacketType.Ack;
            ackMessage.Interface = 0xff;
            ackMessage.Function = 0xffffffff;
            ackMessage.MessageID = 0xffffffff;
            ackMessage.Payload = null;
            ackMessage.PayloadLength = 0;
            //
            //  Create the Ready packet.
            //
            readyMessage.MessageType = (byte) PacketType.Ready;
            readyMessage.Interface = 0xff;
            readyMessage.Function = 0xffffffff;
            readyMessage.MessageID = 0xffffffff;
            readyMessage.Payload = null;
            readyMessage.PayloadLength = 0;
            byte[] encodedReadyPacket = Encoders.EncodeMessage(readyMessage);
            //
            //  Set the state pin high to artifically kick off the process.
            //  this is temporary and only for initial testing.
            //
            Console.WriteLine("Starting reception process.");
            _stm32ReadyPin.State = false;
            _stm32ReadyPin.State = true;
            Console.WriteLine("Sending Ready packet.");
            Console.WriteLine("Sending:");
            Console.WriteLine($"{DebugHelpers.PrintableBuffer(encodedReadyPacket)}");
            //
            //  So we now have a transport packet, wait for ESP32 to be ready and then
            //  toggle our ready pin and start the transmission process.
            //
            WaitForEsp32ToBeReady();
            encodedPacketFromEsp32 = _esp32.WriteRead(encodedReadyPacket, (ushort) encodedReadyPacket.Length);
            Console.WriteLine("Received from ESP32:");
            Console.WriteLine($"{DebugHelpers.PrintableBuffer(encodedPacketFromEsp32)}");
            Console.WriteLine();
            _stm32ReadyPin.State = false;
            //
            //  The encoded frame should contain the header for the message the ESP32 wants to send.
            //  Setup a dummy frame to send and then 
            //
            Messages.Message headerMessage = Encoders.ExtractMessage(encodedPacketFromEsp32);
            DisplayMessage(headerMessage);
            //
            //  Next step, tell the ESP32 we are ready and then wait for ESP32 to be ready.
            //
            _stm32ReadyPin.State = true;
            WaitForEsp32ToBeReady();
            //
            //  Now send the ACK message.
            //
            ackMessage.MessageID = headerMessage.MessageID;
            ackMessage.Interface = headerMessage.Interface;
            ackMessage.Function = headerMessage.Function;
            encodedAckMessage = Encoders.EncodeMessage(ackMessage);
            Console.WriteLine("Sending ACK for message header:");
            Console.WriteLine($"{DebugHelpers.PrintableBuffer(encodedAckMessage)}");
            Console.WriteLine();
            _esp32.WriteBytes(encodedAckMessage);
            ackMessage.Interface = 0xff;
            ackMessage.Function = 0xffffffff;
            ackMessage.MessageID = 0xffffffff;
            _stm32ReadyPin.State = false;
            //
            //  Next up, the message body.
            //
            Console.WriteLine("Asking for message body.");
            byte[] dummyMessage = new byte[Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE + headerMessage.PayloadLength)];
            Array.Clear(dummyMessage, 0, dummyMessage.Length);

            _stm32ReadyPin.State = true;
            WaitForEsp32ToBeReady();
            byte[] encodedMessage = _esp32.WriteRead(dummyMessage, (ushort) dummyMessage.Length);
            Console.WriteLine("Received:");
            Console.WriteLine($"{DebugHelpers.PrintableBuffer(encodedMessage)}");
            Console.WriteLine();
            Messages.Message message = Encoders.ExtractMessage(encodedMessage);
            DisplayMessage(message);
            Messages.AntennaInfo antennaInfo = Encoders.ExtractAntennaInfo(message.Payload, 0);
            Console.WriteLine($"    Antenna: {antennaInfo.Antenna}");
            _stm32ReadyPin.State = false;
            //
            //  Now we need to ACK the message body.
            //
            ackMessage.MessageID = headerMessage.MessageID;
            ackMessage.Interface = headerMessage.Interface;
            ackMessage.Function = headerMessage.Function;
            encodedAckMessage = Encoders.EncodeMessage(ackMessage);
            Console.WriteLine("Sending ACK for message body:");
            Console.WriteLine($"{DebugHelpers.PrintableBuffer(encodedAckMessage)}");
            Console.WriteLine();
            _stm32ReadyPin.State = true;
            WaitForEsp32ToBeReady();
            _esp32.WriteBytes(encodedAckMessage);
            ackMessage.Interface = 0xff;
            ackMessage.Function = 0xffffffff;
            ackMessage.MessageID = 0xffffffff;
            _stm32ReadyPin.State = false;

            Console.WriteLine();
            Console.WriteLine("Waiting for button press.");
        }

        private void _button_Changed(object sender, DigitalInputPortEventArgs e)
        {
            // SendMessage();
            ReceiveMessage();
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
            _startMessage.MessageType = (byte) PacketType.Header;
            _startMessage.Function = 0;
            _startMessage.StatusCode = 0;
            _startMessage.PayloadLength = 0;
            _startMessage.Crc = 0;
            _startMessage.Payload = null;

            Messages.Message dummyTransportMessage = new Messages.Message();
            dummyTransportMessage.Interface = (byte) Esp32Interfaces.Transport;
            dummyTransportMessage.MessageType = (byte)PacketType.Transport;
            dummyTransportMessage.Function = 0;
            dummyTransportMessage.StatusCode = 0;
            dummyTransportMessage.PayloadLength = 0;
            dummyTransportMessage.Crc = 0;
            dummyTransportMessage.Payload = null;
            _dummyTransportPacket = Encoders.EncodeMessage(dummyTransportMessage);

            Thread.Sleep(Timeout.Infinite);
        }
    }
}
