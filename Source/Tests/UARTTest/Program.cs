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

        static void Main(string[] args)
        {
            string comPortName = Environment.GetEnvironmentVariable(FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME);
            if (string.IsNullOrEmpty(comPortName))
            {
                Debug.Print($"Please set the {FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME} environment variable and restart this application.");
            }
            else
            {
                //byte[] testBuffer =
                //{
                //    0xaa, 0xaa, 0xaa, 0x81, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
                //    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6d, 0x34, 0xf5, 0xeb, 0x55
                //};

                UART uart = new UART(comPortName);
                uart.TransportError += UartTransportError;
                uart.DataReceived += UartDataReceived;

                //uart.DecodeSerialData(testBuffer, testBuffer.Length);

                Thread.Sleep(Timeout.Infinite);

                byte[] payload = new byte[45];

                for (byte index = 0; index < payload.Length; index++)
                {
                    payload[index] = index;
                }

                byte sequenceNumber = 0;
                byte[] buffer = new byte[Message.HEADER_LENGTH];
                buffer[Message.INTERFACE] = 0;
                Encoders.EncodeUInt32(0, buffer, Message.FUNCTION_NUMBER);
                Encoders.EncodeUInt32(1, buffer, Message.MESSAGE_ID);
                Encoders.EncodeUInt32(0, buffer, Message.PAYLOAD_LENGTH);

                while (true)
                {
                    //Console.Write("Press enter to send the next message.");
                    //string result = Console.ReadLine();
                    Debug.Print($"Sending message");
                    Frame frame = new Frame(UART.FrameType.ApplicationHeader, 1, sequenceNumber++, buffer);
                    uart.Send(frame.FrameType, frame.MessageID, frame.SequenceNumber, frame.Payload);
                    Thread.Sleep(1500);
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

        private static void UartDataReceived(object sender, UARTDataReceivedEventArgs e)
        {
            Debug.Print($"\nMessage received, frame type: {e.FrameType}, message ID: {e.MessageID}, sequence number: {e.SequenceNumber}, message contents:");
            DebugHelpers.DisplayBufferInHex(e.Frame);
        }

        private static void UartTransportError(object sender, UARTTransportEventArgs e)
        {
            Debug.Print($"Response {e.PacketType} received.");
            if (e.PacketType == UARTTransportEventArgs.TransportPacketType.NAK)
            {
                Debug.Print($"    Status code: {e.StatusCode}");
            }
        }
    }
}
