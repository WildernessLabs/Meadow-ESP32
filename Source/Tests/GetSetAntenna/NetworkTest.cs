using System;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Core.Esp32;

namespace BasicNetworkTest
{
    public class NetworkTest : App<F7Micro, NetworkTest>
    {
        private DigitalInputPort _button;
        
        IDigitalInputPort _esp32UartRx;
        
        IDigitalInputPort _esp32UartTx;

        private UInt32 _counter;

        MessageDispatcher _dispatcher;

        private Messages.Message _getAntennaMessage;

        private Messages.Message _setAntennaMessage;

        private Messages.Message _requestMessage;

        private Messages.AntennaInfo _antennaInfo;

        public NetworkTest()
        {
            _esp32UartRx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_RX, resistorMode: ResistorMode.Disabled);
            _esp32UartTx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX, resistorMode: ResistorMode.Disabled);

//            _dispatcher = new MessageDispatcher(Device, esp32ReadyPin: Device.Pins.ESP_UART_RX,
//                mosi: Device.Pins.MOSI, miso: Device.Pins.MISO, clock: Device.Pins.SCK, chipSelect: Device.Pins.D00,
//                bootPin: Device.Pins.ESP_BOOT, resetPin: Device.Pins.ESP_RST);
            _dispatcher = new MessageDispatcher(Device, esp32ReadyPin: Device.Pins.ESP_BOOT,
                mosi: Device.Pins.ESP_MOSI, miso: Device.Pins.ESP_MISO, clock: Device.Pins.ESP_CLK, chipSelect: Device.Pins.ESP_CS,
                bootPin: Device.Pins.ESP_BOOT, resetPin: Device.Pins.ESP_RST);
            _dispatcher.MessageReceived += _dispatcher_MessageReceived;
            _dispatcher.Error += _dispatcher_Error;
            _button = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.D14, interruptMode: InterruptMode.EdgeFalling, resistorMode: ResistorMode.PullUp, debounceDuration: 500);
            _button.Changed += _button_Changed;

            _antennaInfo = new Messages.AntennaInfo();
            _antennaInfo.Antenna = (byte)AntennaTypes.External;

            _getAntennaMessage = new Messages.Message();
            _getAntennaMessage.Interface = (byte)Esp32Interfaces.WiFi;
            _getAntennaMessage.Function = (byte)WiFiFunction.GetAntenna;
            _getAntennaMessage.MessageID = _counter;
            _getAntennaMessage.MessageType = (byte)MessageTypes.Data;
            _getAntennaMessage.Payload = null;
            _getAntennaMessage.PayloadLength = 0;

            _setAntennaMessage = new Messages.Message();
            _setAntennaMessage.Interface = (byte)Esp32Interfaces.WiFi;
            _setAntennaMessage.Function = (byte)WiFiFunction.SetAntenna;
            _setAntennaMessage.MessageID = _counter;
            _setAntennaMessage.MessageType = (byte)MessageTypes.Data;

            byte[] encodedAntenna = Encoders.EncodeAntennaInfo(_antennaInfo);
            _setAntennaMessage.Payload = encodedAntenna;
            _setAntennaMessage.PayloadLength = (UInt32)encodedAntenna.Length;

            _requestMessage = new Messages.Message();
            _requestMessage.MessageType = (byte)MessageTypes.Transport;
            _requestMessage.Interface = (byte)Esp32Interfaces.Transport;
            _requestMessage.Function = (UInt32)TransportFunction.SendResponse;

            QueueMessage(_getAntennaMessage);
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
            //            QueueMessage(_getAntennaMessage);
            //            QueueMessage(_requestMessage);
            //            QueueMessage(_setAntennaMessage);
            //            QueueMessage(_requestMessage);
            //            QueueMessage(_getAntennaMessage);
            //            QueueMessage(_requestMessage);

            Messages.WiFiCredentials startNetwork = new Messages.WiFiCredentials();
            startNetwork.NetworkName = Secrets.WIFI_NAME;
            startNetwork.Password = Secrets.WIFI_PASSOWRD;

            Messages.Message message = new Messages.Message();
            message.Interface = (byte)Esp32Interfaces.WiFi;
            message.Function = (uint)WiFiFunction.Start;
            message.MessageID = 0x10;
            message.MessageType = (byte)MessageTypes.Data;
            byte[] encodedStartMessage = Encoders.EncodeWiFiCredentials(startNetwork);
            message.Payload = encodedStartMessage;
            message.PayloadLength = (uint)encodedStartMessage.Length;
            
            QueueMessage(message);
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

        private bool webPageRequested = false;

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _dispatcher_MessageReceived(object sender, MessageReceviedEventArgs e)
        {
            DebugHelpers.DebugHelpers.LogMessage("Message received:");
            e.Message.Value.Log();
            switch ((WiFiFunction)e.Message.Value.Function)
            {
                case WiFiFunction.Start:
                    DebugHelpers.DebugHelpers.LogMessage($"Obtained IP address: {e.Message.Value.Payload[0]}.{e.Message.Value.Payload[1]}.{e.Message.Value.Payload[2]}.{e.Message.Value.Payload[3]}");

                    Messages.HttpGetRequest requestData = new Messages.HttpGetRequest();
                    requestData.PortNumber = 80;
                    requestData.ServerName = "www.google.com";
                    requestData.Uri = "/";

                    //                    Messages.Message httpGetRequest = new Messages.Message();
                    //                    httpGetRequest.MessageType = (byte)MessageTypes.Data;
                    //                    httpGetRequest.Interface = (byte)Esp32Interfaces.WiFi;
                    //                    httpGetRequest.Function = (UInt32)WiFiFunction.HttpGet;
                    //                    httpGetRequest.MessageID = 0x02;
                    //                    httpGetRequest.Payload = Encoders.EncodeHttpGetRequest(requestData);
                    //                    httpGetRequest.PayloadLength = (uint)httpGetRequest.Payload.Length;
                    //                    QueueMessage(httpGetRequest);

                    Messages.Message getAccessPointRequest = new Messages.Message();
                    getAccessPointRequest.MessageType = (byte)MessageTypes.Data;
                    getAccessPointRequest.Interface = (byte)Esp32Interfaces.WiFi;
                    getAccessPointRequest.Function = (UInt32)WiFiFunction.GetAccessPoints;
                    getAccessPointRequest.MessageID = 0x02;
                    getAccessPointRequest.PayloadLength = 0;
                    QueueMessage(getAccessPointRequest);
                    break;
                case WiFiFunction.HttpGet:
                    Messages.HttpGetResponse response = Encoders.ExtractHttpGetResponse(e.Message.Value.Payload, 0);
                    string responseText = string.Empty;
                    foreach (byte b in response.Response)
                    {
                        responseText += Convert.ToChar(b);
                    }
                    string[] lines = responseText.Split(new char[] { '\n', '\r' });
                    foreach (string line in lines)
                    {
                        DebugHelpers.DebugHelpers.LogMessage(line);
                    }
                    break;
                case WiFiFunction.GetAccessPoints:
                    Messages.AccessPointList accessPoints = Encoders.ExtractAccessPointList(e.Message.Value.Payload, 0);
                    if (accessPoints.NumberOfAccessPoints > 0)
                    {
                        int offset = 0;
                        DebugHelpers.DebugHelpers.LogMessage("|------------------------------------------");
                        DebugHelpers.DebugHelpers.LogMessage("|         Network Name             | RSSI |");
                        DebugHelpers.DebugHelpers.LogMessage("|------------------------------------------");
                        for (int count = 0; count < accessPoints.NumberOfAccessPoints; count++)
                        {
                            Messages.AccessPoint accessPoint = Encoders.ExtractAccessPoint(accessPoints.AccessPoints, offset);
                            offset += 41;
                            DebugHelpers.DebugHelpers.LogMessage($"| {accessPoint.Ssid,-32} | {accessPoint.Rssi,4} |");
                        }
                    }
                    else
                    {
                        DebugHelpers.DebugHelpers.LogMessage("No access points returned.");
                    }
                    break;
            }
        }

        /// <summary>
        /// Main program loop.
        /// </summary>
        public void Run()
        {
            DebugHelpers.DebugHelpers.LogMessage("Basic Network Communication Test.");
            DebugHelpers.DebugHelpers.LogMessage("Press button to start the test.");
            
            Thread.Sleep(Timeout.Infinite);
        }

    }
}
