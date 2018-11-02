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

        /// <summary>
        ///     Location of the Interface indicator in the message header.
        /// </summary>
        public const int INTERFACE = 0;

        /// <summary>
        ///     Location of the Function bytes (4) in the message header.
        /// </summary>
        public const int FUNCTION_NUMBER = INTERFACE + 1;

        /// <summary>
        ///     Location of the MessageID bytes (4) in the message header
        /// </summary>
        public const int MESSAGE_ID = FUNCTION_NUMBER + 4;

        /// <summary>
        ///     Location of the Payload length bytes (2) in the message header.
        /// </summary>
        public const short PAYLOAD_LENGTH = MESSAGE_ID + 4;

        /// <summary>
        ///     Length of the header in bytes.
        /// </summary>
        public const int HEADER_LENGTH = PAYLOAD_LENGTH + 2;

        static void Main(string[] args)
        {
            string comPortName = Environment.GetEnvironmentVariable(FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME);
            if (string.IsNullOrEmpty(comPortName))
            {
                Console.WriteLine($"Please set the {FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME} environment variable and restart this application.");
            }
            else
            {
                byte[] payload = new byte[45];

                for (byte index = 0; index < payload.Length; index++)
                {
                    payload[index] = index;
                }

                UART uart = new UART(comPortName);

                uart.ResponseReceived += Uart_ResponseReceived;

                byte sequenceNumber = 0;
                byte[] buffer = new byte[HEADER_LENGTH];
                buffer[INTERFACE] = 0;
                Encoders.EncodeUInt32(0, buffer, FUNCTION_NUMBER);
                Encoders.EncodeUInt32(1, buffer, MESSAGE_ID);
                Encoders.EncodeUInt16(0, buffer, PAYLOAD_LENGTH);

                while (true)
                {
                    Console.Write("Press enter to send the next message.");
                    string result = Console.ReadLine();
                    Frame frame = new Frame(UART.FrameType.ApplicationHeader, 1, sequenceNumber++, buffer);
                    uart.Send(frame.FrameType, frame.MessageID, frame.SequenceNumber, frame.Payload);
                    Thread.Sleep(500);
                }

                //MessageQueue queue = new MessageQueue(comPortName);
                //UInt32 counter = 0;

                //while (true)
                //{
                //    counter++;
                //    Message message = new Message(0, 10, counter, payload);
                //    queue.Add(message);
                //    Thread.Sleep(1000);
                //}
            }
        }

        private static void Uart_ResponseReceived(object sender, UARTResponseEventArgs e)
        {
            Console.WriteLine($"Response {e.Packet} received.");
            if (e.Packet == UARTResponseEventArgs.TransportPacketType.NAK)
            {
                Console.WriteLine($"    Status code: {e.StatusCode}");
            }
        }
    }
}
