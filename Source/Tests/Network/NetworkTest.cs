using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Foundation.Sensors.Atmospheric;
using Meadow.Gateway.WiFi;
using Meadow.Hardware;
using System.Collections.ObjectModel;

namespace BasicNetworkTest
{
    public class NetworkTest : App<F7Micro, NetworkTest>
    {
        static HttpClient client;

        static Bme280 bme280;
        static WiFiAdapter wifi;

        /// <summary>
        /// Default constructor.
        /// </summary>
        public NetworkTest()
        {
            Console.WriteLine("Connecting to network.");
            Device.InitWiFiAdapter().Wait();
            if (Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception("Cannot connect to network, application halted.");
            }
            Console.WriteLine("WiFi connection completed.");
        }

        public async Task GetWebPageAsync(string uri)
        {
            Console.WriteLine("********** GetWebPageAsync **********");
            
            client = new HttpClient();
            HttpResponseMessage response = await client.GetAsync(uri);

            response.EnsureSuccessStatusCode();
            string responseBody = await response.Content.ReadAsStringAsync();
            Log(responseBody);
        }

        public void PostData(string uri, float temperature)
        {
            Console.WriteLine("********** PostData **********");
            
            Log($"Connecting to '{uri}'");
            var httpWebRequest = (HttpWebRequest) WebRequest.Create(uri);
            httpWebRequest.ContentType = "application/json";
            httpWebRequest.Method = "POST";
            httpWebRequest.Headers.Add("X-AIO-Key", Secrets.APIO_KEY);

            Log("Creating JSON data.");
            using (var streamWriter = new StreamWriter(httpWebRequest.GetRequestStream()))
            {
                string json = "{\"value\":\"" + temperature.ToString("F2.2") + "\"}";

                streamWriter.Write(json);
            }

            var httpResponse = (HttpWebResponse) httpWebRequest.GetResponse();
            using (var streamReader = new StreamReader(httpResponse.GetResponseStream()))
            {
                Log(streamReader.ReadToEnd());
            }
        }

        public void Log(string message)
        {
            string output = DateTime.Now.ToString("HH:mm:ss");
            output += ": ";
            output += message;
            Console.WriteLine(output);
        }

        public async Task PostDataAsync(string uri)
        {
            Console.WriteLine("********** PostDataAsync **********");
            
            while (true)
            {
                client = new HttpClient();
                client.DefaultRequestHeaders.Add("X-AIO-Key", Secrets.APIO_KEY);
                client.Timeout = TimeSpan.FromMinutes(10);

                string temperature = "23.70";
                Log($"Writing temperature {temperature}C to Adafruit IO.");

                string data = "{\"value\":\"" + temperature + "\"}";
                var content = new StringContent(data, Encoding.UTF8, "application/json");
                var result = client.PostAsync(uri, content).Result;

                client.Dispose();
                client = null;

                //Log("Response:");
                //Log(result);
                Log($"Data sent.");
                Thread.Sleep(10000);
            }
        }

        // Incoming data from the client.
        public static string data = null;

        public void StartListeningUsingSockets()
        {
            Console.WriteLine("********** StartListeningUsingSockets **********");
            
            byte[] bytes = new Byte[1024];

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
