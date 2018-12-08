using System;
using System.Diagnostics;
using System.Text;
using System.Threading;
using Esp32Messaging;
using UARTTransport;

namespace UARTTest
{
    class Program
    {
        private const string FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME = "FTDIComPort";
        private const string SSID_ENVIRONMENT_VARIABLE_NAME = "ESP32NetworkName";
        private const string ESP32_NETWORK_PASSWORD_ENVIRONMENT_VARIABLE_NAME = "ESP32NetworkPassword";
        private const byte WiFiInterface = 0x00;
        private const UInt32 WiFiFunctionStart = 0x00;
        private const UInt32 WiFiFunctionScanForNetworks = 0x03;

        static void Main(string[] args)
        {
            string comPortName = Environment.GetEnvironmentVariable(FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME);
            string ssid = Environment.GetEnvironmentVariable(SSID_ENVIRONMENT_VARIABLE_NAME);
            string networkPassword = Environment.GetEnvironmentVariable(ESP32_NETWORK_PASSWORD_ENVIRONMENT_VARIABLE_NAME);

            bool environmentOK = true;
            if (string.IsNullOrEmpty(comPortName))
            {
                Debug.Print($"Please set the {FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME} environment variable and restart this application.");
                environmentOK = false;
            }
            if (string.IsNullOrEmpty(ssid))
            {
                Debug.Print($"Please set the {SSID_ENVIRONMENT_VARIABLE_NAME} environment variable and restart this application.");
                environmentOK = false;
            }
            if (string.IsNullOrEmpty(networkPassword))
            {
                Debug.Print($"Please set the {ESP32_NETWORK_PASSWORD_ENVIRONMENT_VARIABLE_NAME} environment variable and restart this application.");
                environmentOK = false;
            }
            if (environmentOK)
            {
                byte[] payload = new byte[ssid.Length + networkPassword.Length + 2];
                Array.Copy(Encoding.UTF8.GetBytes(ssid), payload, ssid.Length);
                payload[ssid.Length] = 0;
                Array.Copy(Encoding.UTF8.GetBytes(networkPassword), 0, payload, ssid.Length + 1, networkPassword.Length);
                payload[ssid.Length + networkPassword.Length + 1] = 0;

                MessageQueue queue = new MessageQueue(comPortName);
                queue.MessageReceived += Queue_MessageReceived;
                queue.MessageError += Queue_MessageError;

                UInt32 messageID = 0x01;
                Message message = new Message(WiFiInterface, WiFiFunctionStart, messageID++, payload);
                queue.Add(message);
                Thread.Sleep(5000);
                Message scanForNetworksMessage = new Message(WiFiInterface, WiFiFunctionScanForNetworks, messageID++, null);
                queue.Add(scanForNetworksMessage);

                Thread.Sleep(Timeout.Infinite);
            }
        }

        private static void Queue_MessageError(object sender, MessageQueueErrorEventArgs e)
        {
            Debug.Print("MessageQueue has received an error response.");
        }

        private static void Queue_MessageReceived(object sender, MessageReceviedEventArgs e)
        {
            switch (e.Esp32Message.MessageID)
            {
                case 0x01:
                    Debug.WriteLine("WiFi Started.");
                    Debug.WriteLine($"    IP Address: {e.Esp32Message.Payload[0]}.{e.Esp32Message.Payload[1]}.{e.Esp32Message.Payload[2]}.{e.Esp32Message.Payload[3]}");
                    break;
                case 0x02:
                    Debug.WriteLine("Access point list retrieved");
                    UInt16 numberOfAccessPoints = 0;
                    UInt16 sizeOfWiFiAccessPointInformation = 0;
                    if (e.Esp32Message.Payload.Length > 0)
                    {
                        numberOfAccessPoints = Encoders.ExtractUInt16(e.Esp32Message.Payload, 0);
                        sizeOfWiFiAccessPointInformation = Encoders.ExtractUInt16(e.Esp32Message.Payload, 2);
                    }
                    Debug.WriteLine($"    Number of access points: {numberOfAccessPoints}");
                    Debug.WriteLine($"    Size of WiFi Access Point structure {sizeOfWiFiAccessPointInformation}");
                    break;
            }
        }
    }
}
