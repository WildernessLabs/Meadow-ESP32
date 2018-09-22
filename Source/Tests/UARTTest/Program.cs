using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UARTTest
{
    class Program
    {
        const int BUFFER_SIZE = 1024;
        private const string FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME = "FTDIComPort";
        private static object comPortLock = new object();
        private static bool canSend = true;

        static void Main(string[] args)
        {
            string comPortName = Environment.GetEnvironmentVariable(FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME);
            if (string.IsNullOrEmpty(comPortName))
            {
                Console.WriteLine($"Please set the {FTDI_COM_PORT_ENVIRONMENT_VARIABLE_NAME} environment variable and restart this application.");
            }
            else
            {
                SerialPort comPort = new SerialPort(comPortName.ToUpper(), 115200, Parity.None, 8, StopBits.One);
                comPort.DataReceived += ComPort_DataReceived;
                UInt32 counter = 0;
                byte[] message = new byte[45];

                for (byte index = 0; index < message.Length; index++)
                {
                    message[index] = index;
                }

                while (true)
                {
                    counter++;
                    byte[] packet = Message.CreateHeader(0, 10, counter);
                    comPort.Open();
                    //
                    comPort.Write(new byte[] { 0xaa, 0xaa, 0xaa }, 0, 3);
                    comPort.Write(packet, 0, packet.Length);
                    comPort.Write(new byte[] { 0x55 }, 0, 1);
                    //
                    packet = Message.CreateDataPacket(message);
                    comPort.Write(new byte[] { 0xaa, 0xaa, 0xaa }, 0, 3);
                    comPort.Write(packet, 0, packet.Length);
                    comPort.Write(new byte[] { 0x55 }, 0, 1);
                    //
                    packet = Message.CreateHeader(0, 11, counter);
                    comPort.Write(new byte[] { 0xaa, 0xaa, 0xaa }, 0, 3);
                    comPort.Write(packet, 0, packet.Length);
                    comPort.Write(new byte[] { 0x55 }, 0, 1);
                    //
                    packet = Message.CreateDataPacket(message, 0, 20);
                    comPort.Write(new byte[] { 0xaa, 0xaa, 0xaa }, 0, 3);
                    comPort.Write(packet, 0, packet.Length);
                    comPort.Write(new byte[] { 0x55 }, 0, 1);
                    //
                    packet = Message.CreateDataPacket(message, 20, message.Length - 20);
                    comPort.Write(new byte[] { 0xaa, 0xaa, 0xaa }, 0, 3);
                    comPort.Write(packet, 0, packet.Length);
                    comPort.Write(new byte[] { 0x55 }, 0, 1);
                    //
                    comPort.Close();
                    Thread.Sleep(1000);
                }
            }
        }

        private static void ComPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            string str;
            str = "";
            if (e.EventType == SerialData.Chars)
            {
                int amount;
                byte[] buffer;

                buffer = new byte[BUFFER_SIZE];
                amount = ((SerialPort)sender).Read(buffer, 0, BUFFER_SIZE);
                if (amount > 0)
                {
                    for (int index = 0; index < amount; index++)
                    {
                        str += buffer[index].ToString();
                        str += " ";
                    }
                    Console.WriteLine("Received: {0}", str);
                }
            }
        }
    }
}
