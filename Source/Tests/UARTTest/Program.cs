using System;
using System.Diagnostics;
using System.Text;
using System.Threading;
using Esp32Messaging;
using UartTransport;
using System.Collections.Generic;

namespace UartTest
{
    class Program
    {
        private const string FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME = "FTDIComPort";
        private const string SSID_ENVIRONMENT_VARIABLE_NAME = "ESP32NetworkName";
        private const string ESP32_NETWORK_PASSWORD_ENVIRONMENT_VARIABLE_NAME = "ESP32NetworkPassword";
        private const byte WiFiInterface = 0x00;
        private const UInt32 WiFiFunctionStart = 0x00;
        private const UInt32 WiFiFunctionScanForNetworks = 0x03;
        //
        //  Constants for the HTTP Get request test.
        //
        private const string ServerName = "google.com";
        private const string Uri = "http://google.com/";
        private const UInt16 PortNumber = 443;
        private const UInt32 WiFiFunctionHttpGet = 0x05;

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
                int destination = 0;
                Array.Copy(Encoding.UTF8.GetBytes(ssid), payload, ssid.Length);
                destination = ssid.Length;
                payload[destination] = 0;
                destination++;
                Array.Copy(Encoding.UTF8.GetBytes(networkPassword), 0, payload, destination, networkPassword.Length);
                destination += networkPassword.Length;
                payload[destination] = 0;

                MessageQueue queue = new MessageQueue(comPortName);
                queue.MessageReceived += Queue_MessageReceived;
                queue.MessageError += Queue_MessageError;

                UInt32 messageID = WiFiFunctionStart;
                Message message = new Message(WiFiInterface, WiFiFunctionStart, messageID, payload);
                queue.Add(message);
                Thread.Sleep(5000);
                messageID = WiFiFunctionScanForNetworks;
                Message scanForNetworksMessage = new Message(WiFiInterface, WiFiFunctionScanForNetworks, messageID, null);
                queue.Add(scanForNetworksMessage);
                Thread.Sleep(5000);
                //
                //  Prepare a Get request.
                //
                byte[] httpGetRequestPayload = new byte[ServerName.Length + Uri.Length + 4];
                Encoders.EncodeUInt16(PortNumber, httpGetRequestPayload, 0);
                destination = 2;
                Array.Copy(Encoding.UTF8.GetBytes(ServerName), 0, httpGetRequestPayload, destination, ServerName.Length);
                destination += ServerName.Length;
                httpGetRequestPayload[destination] = 0;
                destination++;
                Array.Copy(Encoding.UTF8.GetBytes(Uri), 0, httpGetRequestPayload, destination, Uri.Length);
                destination += Uri.Length;
                httpGetRequestPayload[destination] = 0;
                messageID = WiFiFunctionHttpGet;
                Message httpGetRequest = new Message(WiFiInterface, WiFiFunctionHttpGet, messageID, httpGetRequestPayload);
                queue.Add(httpGetRequest);

                Thread.Sleep(Timeout.Infinite);
            }
        }

        private const int BSSID_OFFSET = 0;
        private const int SSID_OFFSET = BSSID_OFFSET + 6;
        private const int PRIMARY_CHANNEL_OFFSET = SSID_OFFSET + 33;
        private const int SECONDARY_CHANNEL_OFFSET = PRIMARY_CHANNEL_OFFSET + 1;
        private const int RSSI_OFFSET = SECONDARY_CHANNEL_OFFSET + 1;
        private const int AUTHENTICATION_MODE_OFFSET = RSSI_OFFSET + 1;
        private const int PAIRWISE_CIPHER_OFFSET = AUTHENTICATION_MODE_OFFSET + 1;
        private const int GROUP_CIPHER_OFFSET = PAIRWISE_CIPHER_OFFSET + 1;
        private const int ANTENNA_OFFSET = GROUP_CIPHER_OFFSET + 1;
        private const int PROTOCOLS_OFFSET = ANTENNA_OFFSET + 1;
        private const int COUNTRY_CODE_OFFSET = PROTOCOLS_OFFSET + 4;
        private const int START_CHANNEL_OFFSET = COUNTRY_CODE_OFFSET + 3;
        private const int NUMBER_OF_CHANNELS_OFFSET = START_CHANNEL_OFFSET + 1;
        private const int TX_POWER_MAXIMUM_OFFSET = NUMBER_OF_CHANNELS_OFFSET + 1;
        private const int POLICY_OFFSET = TX_POWER_MAXIMUM_OFFSET + 1;
        private const int ACCESS_POINT_INFORMATION_LENGTH = POLICY_OFFSET + 1;

        private static List<AccessPoint> BuildAccessPointsList(UInt16 numberOfAccessPoints, byte[] payload)
        {
            List<AccessPoint> accessPoints = new List<AccessPoint>(numberOfAccessPoints);
            int offset = 2;
            for (int accessPoint = 0; accessPoint < numberOfAccessPoints; accessPoint++)
            {
                AccessPoint ap = new AccessPoint();

                byte[] bssid = new byte[6];
                Array.Copy(payload, offset + BSSID_OFFSET, bssid, 0, 6);
                ap.Bssid = new MacAddress(bssid);
                byte[] ssid = new byte[33];
                Array.Copy(payload, offset + SSID_OFFSET, ssid, 0, 33);
                char[] apName = Encoding.UTF8.GetChars(ssid);
                int index = 0;
                while ((index < 33) && (ssid[index] != 0))
                {
                    ap.Ssid += apName[index];
                    index++;
                }
                ap.PrimaryChannel = payload[offset + PRIMARY_CHANNEL_OFFSET];
                ap.SecondaryChannel = (WiFiSecondChannel) payload[offset + SECONDARY_CHANNEL_OFFSET];
                ap.Rssi = (sbyte) payload[offset + RSSI_OFFSET];
                ap.AuthenticationMode = (WiFiAuthenticationMode) payload[offset + AUTHENTICATION_MODE_OFFSET];
                accessPoints.Add(ap);
                offset += ACCESS_POINT_INFORMATION_LENGTH;
            }
            return (accessPoints);
        }

        private static void Queue_MessageError(object sender, MessageQueueErrorEventArgs e)
        {
            Debug.Print("MessageQueue has received an error response.");
        }

        private static void Queue_MessageReceived(object sender, MessageReceviedEventArgs e)
        {
            switch (e.Esp32Message.MessageID)
            {
                case WiFiFunctionStart:
                    Debug.WriteLine("WiFi Started.");
                    Debug.WriteLine($"    IP Address: {e.Esp32Message.Payload[0]}.{e.Esp32Message.Payload[1]}.{e.Esp32Message.Payload[2]}.{e.Esp32Message.Payload[3]}");
                    break;
                case WiFiFunctionScanForNetworks:
                    Debug.WriteLine("Access point list retrieved");
                    UInt16 numberOfAccessPoints = 0;
                    if (e.Esp32Message.Payload.Length > 0)
                    {
                        numberOfAccessPoints = Encoders.ExtractUInt16(e.Esp32Message.Payload, 0);
                        Debug.WriteLine($"    Number of access points: {numberOfAccessPoints}");
                        List<AccessPoint> accessPoints = BuildAccessPointsList(numberOfAccessPoints, e.Esp32Message.Payload);
                        Debug.WriteLine("    ===============================================================");
                        Debug.WriteLine("    |             SSID                 |    MAC Address    | RSSI |");
                        Debug.WriteLine("    ===============================================================");
                        foreach (AccessPoint accessPoint in accessPoints)
                        {
                            Debug.WriteLine($"    | {accessPoint.Ssid,32} | {accessPoint.Bssid.ToString(),17} | {accessPoint.Rssi,4} |");
                        }
                    }
                    else
                    {
                        Debug.WriteLine("    No access points detected.");
                    }
                    break;
                case WiFiFunctionHttpGet:
                    Debug.WriteLine("HTTP Get responded.");
                    break;
                default:
                    Debug.WriteLine($"Unknown message ID {e.Esp32Message.MessageID}");
                    break;
            }
        }
    }
}
