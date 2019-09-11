using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Core.Esp32;
using System.Threading;

namespace BasicNetworkTest
{
    public class BasicNetworkTestApplication : App<F7Micro, BasicNetworkTestApplication>
    {
        private DigitalInputPort _button;

        private UInt32 _counter;

        MessageDispatcher _dispatcher;

        private Messages.Message _getAntennaMessage;

        private Messages.Message _setAntennaMessage;

        private Messages.Message _requestMessage;

        private Messages.AntennaInfo _antennaInfo;

        /// <summary>
        /// Default constructor.
        /// </summary>
        public BasicNetworkTestApplication()
        {
            _dispatcher = new MessageDispatcher(Device, esp32ReadyPin: Device.Pins.ESP_UART_RX, 
                mosi: Device.Pins.ESP_MOSI, miso: Device.Pins.ESP_MISO, clock: Device.Pins.ESP_CLK, chipSelect: Device.Pins.ESP_CS, 
                bootPin: Device.Pins.ESP_BOOT, resetPin: Device.Pins.ESP_RST);
            _dispatcher.MessageReceived += _dispatcher_MessageReceived;
            _dispatcher.Error += _dispatcher_Error;
            _button = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.D14, interruptMode: InterruptMode.EdgeFalling, resistorMode: ResistorMode.PullUp, debounceDuration: 500);
            _button.Changed += _button_Changed;
            
            _antennaInfo = new Messages.AntennaInfo();
            _antennaInfo.Antenna = (byte) AntennaTypes.External;

            _getAntennaMessage = new Messages.Message();
            _getAntennaMessage.Interface = (byte) Esp32Interfaces.WiFi;
            _getAntennaMessage.Function = (byte) WiFiFunction.GetAntenna;
            _getAntennaMessage.MessageID = _counter;
            _getAntennaMessage.MessageType = (byte) MessageTypes.Data;
            _getAntennaMessage.Payload = null;
            _getAntennaMessage.PayloadLength = 0;
            
            _setAntennaMessage = new Messages.Message();
            _setAntennaMessage.Interface = (byte) Esp32Interfaces.WiFi;
            _setAntennaMessage.Function = (byte) WiFiFunction.SetAntenna;
            _setAntennaMessage.MessageID = _counter;
            _setAntennaMessage.MessageType = (byte) MessageTypes.Data;

            byte[] encodedAntenna = Encoders.EncodeAntennaInfo(_antennaInfo);
            _setAntennaMessage.Payload = encodedAntenna;
            _setAntennaMessage.PayloadLength = (UInt32) encodedAntenna.Length;
            
            _requestMessage = new Messages.Message();
            _requestMessage.MessageType = (byte)MessageTypes.Transport;
            _requestMessage.Interface = (byte)Esp32Interfaces.Transport;
            _requestMessage.Function = (UInt32)TransportFunction.SendResponse;
        }

        private void QueueMessage(Messages.Message message)
        {
            message.MessageID = _counter;
            _counter++;
            _dispatcher.Add(message);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _button_Changed(object sender, DigitalInputPortEventArgs e)
        {
            DebugHelpers.DebugHelpers.LogMessage("Queueing messages");
            QueueMessage(_getAntennaMessage);
            QueueMessage(_requestMessage);
            QueueMessage(_setAntennaMessage);
            QueueMessage(_requestMessage);
            QueueMessage(_getAntennaMessage);
            QueueMessage(_requestMessage);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _dispatcher_Error(object sender, MessageDispatcherErrorEventArgs e)
        {
            DebugHelpers.DebugHelpers.LogMessage($"Dispatcher error: {e.StatusCode}");
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _dispatcher_MessageReceived(object sender, MessageReceviedEventArgs e)
        {
            DebugHelpers.DebugHelpers.LogMessage("Message received:");
            e.Message.Value.Log();
        }

        /// <summary>
        /// Main program loop.
        /// </summary>
        public void Run()
        {
            DebugHelpers.DebugHelpers.LogMessage("Basic Network Communication Test.");
            DebugHelpers.DebugHelpers.LogMessage("Press button to start the test.");
            
            Messages.StartNetwork startNetwork = new Messages.StartNetwork();
            startNetwork.NetworkName = Secrets.WIFI_NAME;
            startNetwork.Password = Secrets.WIFI_PASSOWRD;
            
            Messages.Message message = new Messages.Message();
            message.Interface = (byte) Esp32Interfaces.WiFi;
            message.Function = (uint) WiFiFunction.Start;
            message.MessageID = 0x10;
            message.MessageType = (byte) MessageTypes.Data;
            byte[] encodedStartMessage = Encoders.EncodeStartNetwork(startNetwork);
            message.Payload = encodedStartMessage;
            message.PayloadLength = (uint) encodedStartMessage.Length;
            QueueMessage(message);
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
