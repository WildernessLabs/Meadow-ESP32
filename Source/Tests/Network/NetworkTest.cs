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
            Console.WriteLine("Connection request completed.");
        }

        public async Task GetWebPageAsync(string uri)
        {
            client = new HttpClient();
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
            string output = DateTime.Now.ToString("HH:mm:ss");
            output += ": ";
            output += message;
            Console.WriteLine(output);
        }

        public async Task PostDataAsync(string uri)
        {
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

        /// <summary>
        ///
        /// </summary>
        public void WebSocketPost()
        {
            byte[] bytes = new byte[2048];

            var i2c = Device.CreateI2cBus();
            bme280 = new Bme280(i2c, Bme280.I2cAddress.Adddress0x77);
            bme280.StartUpdating();
            const int MAXIMUM_READING_COUNT = 12;
            int currentReadingCount = 0;
            bool errorEncountered = false;

            IPHostEntry ipHostInfo = Dns.GetHostEntry("io.adafruit.com");
            IPAddress ipAddress = ipHostInfo.AddressList[0];
            IPEndPoint remoteEP = new IPEndPoint(ipAddress, 80);

            while (true)
            {
                if ((currentReadingCount == MAXIMUM_READING_COUNT) || errorEncountered)
                {
                    Thread.Sleep(5000);
                    if (wifi.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).ConnectionStatus != ConnectionStatus.Success)
                    {
                        throw new Exception("Cannot connect to network, applicaiton halted.");
                    }
                    currentReadingCount = 0;
                }
                errorEncountered = false;
                currentReadingCount++;
                Console.WriteLine($"Logging dataset {currentReadingCount}");
                try
                {
                    Socket sender = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
                    sender.SendTimeout = 3000;
                    sender.ReceiveTimeout = 3000;

                    sender.Connect(remoteEP);

                    #region Send data.

                    Log($"Sending data to Adatfruit IO.");
                    int bytesSent;
                    int bytesReceived;
                    //
                    //  Temperature
                    //
                    Log("Sending temperature data.");
                    string temperatureData = "{\"value\":\"" + bme280.Temperature.ToString("F2") + "\"}";
                    string temperatureHeader = "POST /api/v2/nevyn/feeds/airtemperature/data HTTP/1.1\r\n";
                    temperatureHeader += $"X-AIO-Key: {Secrets.APIO_KEY}\r\n";
                    temperatureHeader += "Content-Type: application/json; charset=utf-8\r\n";
                    temperatureHeader += $"Content-Length: {temperatureData.Length}\r\n";
                    temperatureHeader += "Host: io.adafruit.com\r\n";
                    temperatureHeader += "\r\n";
                    bytesSent = sender.Send(Encoding.ASCII.GetBytes(temperatureHeader));
                    bytesSent += sender.Send(Encoding.ASCII.GetBytes(temperatureData));
                    bytesReceived = sender.Receive(bytes);
                    //
                    //  Humidity
                    //
                    Log("Sending humidity data.");
                    string humidityData = "{\"value\":\"" + bme280.Humidity.ToString("F2") + "\"}";
                    string humidityHeader = "POST /api/v2/nevyn/feeds/humidity/data HTTP/1.1\r\n";
                    humidityHeader += $"X-AIO-Key: {Secrets.APIO_KEY}\r\n";
                    humidityHeader += "Content-Type: application/json; charset=utf-8\r\n";
                    humidityHeader += $"Content-Length: {humidityData.Length}\r\n";
                    humidityHeader += "Host: io.adafruit.com\r\n";
                    humidityHeader += "\r\n";
                    bytesSent = sender.Send(Encoding.ASCII.GetBytes(humidityHeader));
                    bytesSent += sender.Send(Encoding.ASCII.GetBytes(humidityData));
                    bytesReceived = sender.Receive(bytes);
                    //
                    //  Air pressure
                    //
                    Log("Sending air pressure data.");
                    string pressureData = "{\"value\":\"" + bme280.Pressure.ToString("F0") + "\"}";
                    string pressureHeader = "POST /api/v2/nevyn/feeds/airpressure/data HTTP/1.1\r\n";
                    pressureHeader += $"X-AIO-Key: {Secrets.APIO_KEY}\r\n";
                    pressureHeader += "Content-Type: application/json; charset=utf-8\r\n";
                    pressureHeader += $"Content-Length: {pressureData.Length}\r\n";
                    pressureHeader += "Host: io.adafruit.com\r\n";
                    pressureHeader += "\r\n";
                    bytesSent = sender.Send(Encoding.ASCII.GetBytes(pressureHeader));
                    bytesSent += sender.Send(Encoding.ASCII.GetBytes(pressureData));
                    bytesReceived = sender.Receive(bytes);

                    #endregion

                    Log($"Memory available: {GC.GetTotalMemory(false)}");

                    sender.Shutdown(SocketShutdown.Both);

                    sender.Close();

                    Thread.Sleep(720000);
                }
                catch (Exception e)
                {
                    Log($"Unexpected exception : {e.ToString()}");
                    errorEncountered = true;
                }
            }
        }

        public void WebSocketPostTestData()
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

                        string temperature = testData.GetNext().ToString("F2");
                        Log($"Sending data to Adafruit IO.");
                        int bytesSent;
                        int bytesReceived;
                        Log("Sending temperature data.");
                        string temperatureData = "{\"value\":\"" + temperature + "\"}";
                        string temperatureHeader = "POST /v2/MarkAtWildernessLabs/feeds/temperaturetest HTTP/1.1\r\n";
                        temperatureHeader += $"X-AIO-Key: {Secrets.APIO_KEY}\r\n";
                        temperatureHeader += "Content-Type: application/json; charset=utf-8\r\n";
                        temperatureHeader += $"Content-Length: {temperatureData.Length}\r\n";
                        temperatureHeader += "Host: io.adafruit.com\r\n";
                        temperatureHeader += "\r\n";
                        bytesSent = sender.Send(Encoding.ASCII.GetBytes(temperatureHeader));
                        bytesSent += sender.Send(Encoding.ASCII.GetBytes(temperatureData));
                        bytesReceived = sender.Receive(bytes);

                        Log($"Memory available: {GC.GetTotalMemory(false)}");
                        Log("Shutting down the socket.");
                        sender.Shutdown(SocketShutdown.Both);

                        Log("Closing the socket.");
                        sender.Close();

                        Thread.Sleep(15000);
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
                Device.WiFiAdapter.Scan();
                if (Device.WiFiAdapter.Networks.Count > 0)
                {
                    Log("|-------------------------------------------------------------|---------|");
                    Log("|         Network Name             | RSSI |       BSSID       | Channel |");
                    Log("|-------------------------------------------------------------|---------|");
                    foreach (WifiNetwork accessPoint in Device.WiFiAdapter.Networks)
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
