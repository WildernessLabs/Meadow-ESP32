using System;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace UDPClient
{
    class MainClass
    {
        public static void Main(string[] args)
        {
            try
            {
                UdpClient udpClient = new UdpClient();
                Byte[] sendBytes = Encoding.ASCII.GetBytes("Is anybody there?");

                //udpClient.Send(sendBytes, sendBytes.Length);

                udpClient.Send(sendBytes, sendBytes.Length, "192.168.1.200", 11000);

                udpClient.Close();
            }
            catch (Exception e)
            {
                Console.WriteLine(e.ToString());
            }
        }
    }
}
