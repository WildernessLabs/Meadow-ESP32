using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO.Ports;
using System.Linq;
using System.Net;
using System.Reflection.Metadata.Ecma335;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using UARTTransport;
using Esp32Messaging;

namespace UARTTest
{
    class Program
    {
        private const string FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME = "FTDIComPort";
        private const string SSID_ENVIRONMENT_VARIABLE_NAME = "ESP32NetworkName";
        private const string ESP32_NETWORK_PASSWORD_ENVIRONMENT_VARIABLE_NAME = "ESP32NetworkPassword";

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

                Message message = new Message(0, 0, 1, payload);
                while (true)
                {
                    queue.Add(message);
                    Thread.Sleep(5000);
                }
            }
        }
    }
}
