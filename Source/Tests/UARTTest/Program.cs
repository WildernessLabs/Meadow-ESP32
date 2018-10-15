using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Net;
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

        static void Main(string[] args)
        {
            string comPortName = Environment.GetEnvironmentVariable(FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME);
            if (string.IsNullOrEmpty(comPortName))
            {
                Console.WriteLine($"Please set the {FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME} environment variable and restart this application.");
            }
            else
            {
                MessageQueue queue = new MessageQueue(comPortName);
                UInt32 counter = 0;
                byte[] payload = new byte[45];

                for (byte index = 0; index < payload.Length; index++)
                {
                    payload[index] = index;
                }

                while (true)
                {
                    counter++;
                    Message message = new Message(0, 10, counter, payload);
                    queue.Add(message);
                    Thread.Sleep(1000);
                }
            }
        }
    }
}
