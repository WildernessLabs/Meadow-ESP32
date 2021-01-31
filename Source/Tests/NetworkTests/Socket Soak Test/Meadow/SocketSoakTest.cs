using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class SocketSoakTest : App<F7Micro, SocketSoakTest>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public SocketSoakTest()
        {
            Console.WriteLine("Connecting to network.");
            Device.InitWiFiAdapter().Wait();
            if (Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception("Cannot connect to network, application halted.");
            }
            Console.WriteLine("WiFi connection completed.");
        }

        public void Log(string message)
        {
            string output = DateTime.Now.ToString("HH:mm:ss");
            output += ": ";
            output += message;
            Console.WriteLine(output);
        }


        public void RunTest()
        {
            byte[] bytes = new byte[2048];

            try
            {
                Log("Looking up server IP address.");
                IPAddress ipAddress = IPAddress.Parse("192.168.1.197");
                IPEndPoint remoteEP = new IPEndPoint(ipAddress, 11000);

                TestData testData = new TestData();
                int dataPoint = 0;

                Socket sender = null;
                try
                {
                    Log("Creating socket.");
                    sender = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
                    sender.SendTimeout = 3000;
                    sender.ReceiveTimeout = 3000;

                    Log("Connecting to endpoint.");
                    sender.Connect(remoteEP);

                    while (true)
                    {
                        dataPoint++;
                        Log($"Sending data point {dataPoint}");
                        string temperature = testData.GetNext().ToString("F2");
                        Log($"Sending temperature data {temperature} C.");
                        string temperatureData = "{\"value\":\"" + temperature + "\"}\n";
                        int bytesSent = sender.Send(Encoding.ASCII.GetBytes(temperatureData));
                        int bytesReceived = sender.Receive(bytes);

                        string result = Encoding.UTF8.GetString(bytes);
                        Console.WriteLine(result);

                        Log($"Memory allocated: {GC.GetTotalMemory(false)}");
                    }
                }
                catch (ArgumentNullException ane)
                {
                    Log($"ArgumentNullException : {ane.ToString()}");
                }
                catch (SocketException se)
                {
                    Log($"SocketException : {se.ToString()}");
                }
                catch (Exception e)
                {
                    Log($"Unexpected exception : {e.ToString()}");
                }
                finally
                {
                    Log("Shutting down the socket.");
                    sender.Shutdown(SocketShutdown.Both);

                    Log("Closing the socket.");
                    sender.Close();
                }

            }
            catch (Exception e)
            {
                Log(e.ToString());
            }
        }
    }
}
