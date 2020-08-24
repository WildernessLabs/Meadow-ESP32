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

        //static Bme280 bme280;
        static WiFiAdapter wifi;

        /// <summary>
        /// Default constructor.
        /// </summary>
        public NetworkTest()
        {
            //var i2c = Device.CreateI2cBus();
            //bme280 = new Bme280(i2c, Bme280.I2cAddress.Adddress0x77);
            //bme280.StartUpdating();

            //WifiNetwork wifi = new WifiNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD, NetworkType.Any, PhyType.Unknown,
            //    new NetworkSecuritySettings(NetworkAuthenticationType.Wpa2Psk, NetworkEncryptionType.None),
            //    6, NetworkProtocol.Protocol11G, -76);
            Console.WriteLine("Connecting to network.");
            wifi = new WiFiAdapter(new Esp32Coprocessor());
            wifi.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            Console.WriteLine("Connection request completed.");
        }

        public async Task GetWebPageAsync(string uri)
        {
            HttpResponseMessage response = await client.GetAsync(uri);

            response.EnsureSuccessStatusCode();
            string responseBody = await response.Content.ReadAsStringAsync();
            Log(responseBody);
        }

        public void PostData(string uri, float temperature)
        {
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
            Console.WriteLine(message);
            //Log(DateTime.Now.ToString("HH:mm:ss") + $": {message}");
        }

        public async Task PostDataAsync(string uri)
        {
            //while (true)
            //{
            //    client = new HttpClient();
            //    client.DefaultRequestHeaders.Add("X-AIO-Key", Secrets.APIO_KEY);
            //    client.Timeout = TimeSpan.FromMinutes(10);

            //    string temperature = bme280.Temperature.ToString("F2");
            //    Log($"Writing temperature {temperature}C to Adafruit IO.");

            //    string data = "{\"value\":\"" + temperature + "\"}";
            //    var content = new StringContent(data, Encoding.UTF8, "application/json");
            //    var result = client.PostAsync(uri, content).Result;

            //    client.Dispose();
            //    client = null;

            //    //Log("Response:");
            //    //Log(result);
            //    Log($"Data sent.");
            //}
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="uri"></param>
        public void WebSocketPost()
        {
            byte[] bytes = new byte[2048];

            Log($"Memory available: {GC.GetTotalMemory(false)}");

            try
            {
                Log("Looking up server IP address.");
                IPHostEntry ipHostInfo = Dns.GetHostEntry("io.adafruit.com");

                IPAddress ipAddress = ipHostInfo.AddressList[0];

                Log($"IP address of server: {ipAddress.ToString()}");
                IPEndPoint remoteEP = new IPEndPoint(ipAddress, 80);

                string header = "POST /api/v2/nevyn/feeds/test/data HTTP/1.1\r\n";
                header += $"X-AIO-Key: {Secrets.APIO_KEY}\r\n";
                header += "Content-Type: application/json; charset=utf-8\r\n";
                header += "Content-Length: 17\r\n";
                header += "Host: io.adafruit.com\r\n";
                header += "\r\n";
                TestData testData = new TestData();

                while (true)
                {
                    try
                    {
                        Log("Creating socket.");
                        Socket sender = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
                        sender.SendTimeout = 3000;
                        sender.ReceiveTimeout = 3000;

                        Log("Connecting to endpoint.");
                        sender.Connect(remoteEP);

                        byte[] encodedHeader = Encoding.ASCII.GetBytes(header);
                        string temperature = testData.GetNext().ToString("F2");
                        Log($"Sending temperature {temperature}C to Adatfruit IO.");

                        string data = "{\"value\":\"" + temperature + "\"}";
                        byte[] encodedData = Encoding.ASCII.GetBytes(data);

                        Log("Sending data.");
                        int bytesSent;
                        bytesSent = sender.Send(encodedHeader);
                        bytesSent += sender.Send(encodedData);

                        int bytesRec = sender.Receive(bytes);
                        Log($"Memory available: {GC.GetTotalMemory(false)}");

                        Log("Shutting down the socket.");
                        sender.Shutdown(SocketShutdown.Both);

                        Log("Closing the socket.");
                        sender.Close();

                        //Thread.Sleep(900000);
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
                }

            }
            catch (Exception e)
            {
                Log(e.ToString());
            }

        }

        // Incoming data from the client.  
        public static string data = null;

        public void StartListening()
        {
            byte[] bytes = new Byte[1024];

            IPHostEntry ipHostInfo = Dns.GetHostEntry(Dns.GetHostName());
            IPAddress ipAddress = ipHostInfo.AddressList[0];
            IPEndPoint localEndPoint = new IPEndPoint(ipAddress, 11000);

            Socket listener = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);

            try
            {
                listener.Bind(localEndPoint);
                //    listener.Listen(10);

                //    while (true)
                //    {
                //        Console.WriteLine("Waiting for a connection...");
                //        Socket handler = listener.Accept();
                //        data = null;

                //        while (true)
                //        {
                //            int bytesRec = handler.Receive(bytes);
                //            data += Encoding.ASCII.GetString(bytes, 0, bytesRec);
                //            if (data.IndexOf("<EOF>") > -1)
                //            {
                //                break;
                //            }
                //        }

                //        Console.WriteLine("Text received : {0}", data);
                //        byte[] msg = Encoding.ASCII.GetBytes(data);

                //        handler.Send(msg);
                //        handler.Shutdown(SocketShutdown.Both);
                //        handler.Close();
                //    }

            }
            catch (Exception e)
            {
                Console.WriteLine(e.ToString());
            }
        }

        public void ScanForAccessPoints()
        {
            while (true)
            {
                Log("Getting list of access points.");
                wifi.Scan();
                if (wifi.Networks.Count > 0)
                {
                    Log("|-------------------------------------------------------------|---------|");
                    Log("|         Network Name             | RSSI |       BSSID       | Channel |");
                    Log("|-------------------------------------------------------------|---------|");
                    foreach (WifiNetwork accessPoint in wifi.Networks)
                    {
                        Log($"| {accessPoint.Ssid,-32} | {accessPoint.SignalDbStrength,4} | {accessPoint.Bssid,17} |   {accessPoint.ChannelCenterFrequency,3}   |");
                    }
                }
                else
                {
                    Log($"No access points detected.");
                }
                Thread.Sleep(15000);
            }
        }
    }
}
