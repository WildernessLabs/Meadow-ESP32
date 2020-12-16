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

        /// <summary>
        ///
        /// </summary>
        public void WebSocketPost()
        {
            Console.WriteLine("********** WebSocketPost **********");
            
            byte[] bytes = new byte[2048];

            var i2c = Device.CreateI2cBus();
            bme280 = new Bme280(i2c, Bme280.I2cAddress.Adddress0x77);
            bme280.StartUpdating();
            const int MAXIMUM_READING_COUNT = 12;
            int currentReadingCount = 0;

            IPHostEntry ipHostInfo = Dns.GetHostEntry("io.adafruit.com");
            IPAddress ipAddress = ipHostInfo.AddressList[0];
            IPEndPoint remoteEP = new IPEndPoint(ipAddress, 80);

            while (true)
            {
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
                    string temperatureData = "{\"value\":\"" + (bme280.Temperature - 2.0).ToString("F2") + "\"}";
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
                }
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

        public static int pageViews = 0;
        public static int requestCount = 0;
        public static HttpListener listener;
        public string url = "http://192.168.1.200/";


        public static string pageData =
            "<!DOCTYPE>" +
            "<html>" +
            "  <head>" +
            "    <title>HttpListener Example</title>" +
            "  </head>" +
            "  <body>" +
            "    <p>Page Views: {0}</p>" +
            "    <form method=\"post\" action=\"shutdown\">" +
            "      <input type=\"submit\" value=\"Shutdown\" {1}>" +
            "    </form>" +
            "  </body>" +
            "</html>";


        public static async Task HandleIncomingConnections()
        {
            bool runServer = true;

            // While a user hasn't visited the `shutdown` url, keep on handling requests
            while (runServer)
            {
                // Will wait here until we hear from a connection
                HttpListenerContext ctx = await listener.GetContextAsync();

                // Peel out the requests and response objects
                HttpListenerRequest req = ctx.Request;
                HttpListenerResponse resp = ctx.Response;

                // Print out some info about the request
                Console.WriteLine("Request #: {0}", ++requestCount);
                Console.WriteLine(req.Url.ToString());
                Console.WriteLine(req.HttpMethod);
                Console.WriteLine(req.UserHostName);
                Console.WriteLine(req.UserAgent);
                Console.WriteLine();

                // If `shutdown` url requested w/ POST, then shutdown the server after serving the page
                if ((req.HttpMethod == "POST") && (req.Url.AbsolutePath == "/shutdown"))
                {
                    Console.WriteLine("Shutdown requested");
                    runServer = false;
                }

                // Make sure we don't increment the page views counter if `favicon.ico` is requested
                if (req.Url.AbsolutePath != "/favicon.ico")
                    pageViews += 1;

                // Write the response info
                string disableSubmit = !runServer ? "disabled" : "";
                byte[] data = Encoding.UTF8.GetBytes(String.Format(pageData, pageViews, disableSubmit));
                resp.ContentType = "text/html";
                resp.ContentEncoding = Encoding.UTF8;
                resp.ContentLength64 = data.LongLength;

                // Write out to the response stream (asynchronously), then close it
                await resp.OutputStream.WriteAsync(data, 0, data.Length);
                resp.Close();
            }
        }

        public void StartHttpServer()
        {
            Console.WriteLine("********** StartHttpServer **********");
            
            if (!HttpListener.IsSupported)
            {
                Console.WriteLine ("HttpListener class is not supported.");
                return;
            }
            
            // Create a Http server and start listening for incoming connections
            listener = new HttpListener();
            listener.Prefixes.Add(url);
            listener.Start();
            Console.WriteLine("Listening for connections on {0}", url);

            Console.WriteLine("Getting listener context.");
            HttpListenerContext context = listener.GetContext();

            Console.WriteLine("Getting request from the context.");
            HttpListenerRequest request = context.Request;

            Console.WriteLine("Getting response from the context context.");
            HttpListenerResponse response = context.Response;

            Console.WriteLine("Getting listener context.");
            string responseString = "<HTML><BODY> Hello world!</BODY></HTML>";
            byte[] buffer = System.Text.Encoding.UTF8.GetBytes(responseString);
            response.ContentLength64 = buffer.Length;
            System.IO.Stream output = response.OutputStream;

            Console.WriteLine("Writing response.");
            output.Write(buffer,0,buffer.Length);

            Console.WriteLine("Closing the output stream.");
            output.Close();

            Console.WriteLine("Getting stopping the listener.");
            listener.Stop();
        }
    }
}
