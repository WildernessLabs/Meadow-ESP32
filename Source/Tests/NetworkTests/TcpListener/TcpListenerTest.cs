using System;
using System.Net;
using System.Net.Sockets;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class NetworkTest : App<F7Micro, NetworkTest>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public NetworkTest()
        {
            Log("Connecting to network.");
            Device.InitWiFiAdapter().Wait();
            if (Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception("Cannot connect to network, application halted.");
            }
            Log("WiFi connection completed.");
        }

        public void Log(string message)
        {
            string output = DateTime.Now.ToString("HH:mm:ss");
            output += ": ";
            output += message;
            Console.WriteLine(output);
        }

        /// <summary>
        /// Code based upon issue 116 (https://github.com/WildernessLabs/Meadow_Issues/issues/116)
        /// reported 12 Oct 2020.
        /// </summary>
        public void TestTcpListener()
        {
            IPEndPoint ipLocalEP = new IPEndPoint(IPAddress.Parse(Secrets.IP_ADDRESS), 11000);
            TcpListener server = new TcpListener(ipLocalEP);
            try
            {
                Console.WriteLine("Starting tcp listener");
                server.Start();
                Console.WriteLine("Listener started.");
                TcpClient client = server.AcceptTcpClient();
                Console.WriteLine("Connected!");

                Byte[] bytes = new Byte[256];
                string data = null;

                Console.WriteLine("Getting network stream.");
                NetworkStream stream = client.GetStream();
                int i;

                while ((i = stream.Read(bytes, 0, bytes.Length)) != 0)
                {
                    data = System.Text.Encoding.ASCII.GetString(bytes, 0, i);
                    Console.WriteLine("Received: {0}", data);
                }
                Console.WriteLine("Closing connection.");
                client.Close();
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error starting TCPListener {e.Message}");
                throw;
            }
        }
    }
}
