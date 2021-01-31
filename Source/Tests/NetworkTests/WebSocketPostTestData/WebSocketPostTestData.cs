using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class WebSocketPostTestDataTest : App<F7Micro, WebSocketPostTestDataTest>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public WebSocketPostTestDataTest()
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
                IPHostEntry ipHostInfo = Dns.GetHostEntry("io.adafruit.com");

                IPAddress ipAddress = ipHostInfo.AddressList[0];

                Log($"IP address of server: {ipAddress.ToString()}");
                IPEndPoint remoteEP = new IPEndPoint(ipAddress, 80);

                TestData testData = new TestData();
                int dataPoint = 0;

                string feedPost = $"POST /api/v2/{Secrets.APIO_USER_NAME}/feeds/{Secrets.APIO_FEED_NAME}/data HTTP/1.1\r\n";

                Socket sender = null;
                try
                {
                    sender = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
                    sender.SendTimeout = 3000;
                    sender.ReceiveTimeout = 3000;

                    Log("Connecting to endpoint.");
                    sender.Connect(remoteEP);

                    while (true)
                    {
                        dataPoint++;
                        Log($"Sending data point {dataPoint}");
                        Log("Creating socket.");
                        string temperature = testData.GetNext().ToString("F2");
                        Log($"Sending data to Adafruit IO.");
                        int bytesSent;
                        int bytesReceived;
                        Log($"Sending temperature data {temperature} C.");
                        string temperatureData = "{\"value\":\"" + temperature + "\"}";
                        string temperatureHeader = feedPost;
                        temperatureHeader += $"X-AIO-Key: {Secrets.APIO_KEY}\r\n";
                        temperatureHeader += "Content-Type: application/json; charset=utf-8\r\n";
                        temperatureHeader += $"Content-Length: {temperatureData.Length}\r\n";
                        temperatureHeader += "Host: io.adafruit.com\r\n";
                        temperatureHeader += "\r\n";
                        bytesSent = sender.Send(Encoding.ASCII.GetBytes(temperatureHeader));
                        bytesSent += sender.Send(Encoding.ASCII.GetBytes(temperatureData));
                        bytesReceived = sender.Receive(bytes);

                        //string result = Encoding.UTF8.GetString(bytes);
                        //Console.WriteLine(result);

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
