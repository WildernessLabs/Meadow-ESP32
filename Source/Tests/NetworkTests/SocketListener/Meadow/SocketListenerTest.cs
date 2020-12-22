using System;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class SocketListenerTest : App<F7Micro, SocketListenerTest>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public SocketListenerTest()
        {
            Console.WriteLine("Connecting to network.");
            Device.InitWiFiAdapter().Wait();
            if (Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception("Cannot connect to network, application halted.");
            }
            Console.WriteLine("WiFi connection completed.");
        }

        public void RunTest()
        {
            byte[] bytes = new Byte[1024];
            string data;

            IPHostEntry ipHostInfo = Dns.GetHostEntry(Dns.GetHostName());
            IPAddress ipAddress = ipHostInfo.AddressList[0];
            IPEndPoint localEndPoint = new IPEndPoint(ipAddress, 11000);

            Console.WriteLine("Creating socket.");
            Socket listener = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);

            try
            {
                Console.WriteLine("Binding socket.");
                listener.Bind(localEndPoint);
                Console.WriteLine("Listening for connections.");
                listener.Listen(10);

                while (true)
                {
                    Console.WriteLine("Accepting connection.");
                    Socket handler = listener.Accept();
                    data = null;

                    while (true)
                    {
                        Console.WriteLine("Receiving data.");
                        int bytesRec = handler.Receive(bytes);
                        data += Encoding.ASCII.GetString(bytes, 0, bytesRec);
                        if (data.IndexOf("<EOF>") > -1)
                        {
                            break;
                        }
                    }

                    Console.WriteLine("Text received : {0}", data);

                    Console.WriteLine("Shutting down the socket.");
                    handler.Shutdown(SocketShutdown.Both);
                    Console.WriteLine("Closing socket.");
                    handler.Close();
                }
            }
            catch (Exception e)
            {
                Console.WriteLine(e.ToString());
            }
        }
    }
}
