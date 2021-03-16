using System;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;
using System.Threading;

namespace MeadowApp
{
    public class HttpListenerTest : App<F7Micro, HttpListenerTest>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public HttpListenerTest()
        {
            Console.WriteLine("Creating HttpListenerTest object.");
            //MeadowOS.WatchdogEnable(25000);
            //StartWatchdog(20000);
            Device.InitWiFiAdapter().Wait();
            Device.WiFiAdapter.WiFiConnected += WiFiAdapter_WiFiConnected;
            Console.WriteLine("Connecting to access point.");
            Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).Wait();
            Console.WriteLine("WiFi connection completed.");
        }

        private void WiFiAdapter_WiFiConnected(object sender, EventArgs e)
        {
            Console.WriteLine("WiFiAdapter_ConnectionCompleted: connected to access point.");
        }

        void StartWatchdog(int interval)
        {
            // just for good measure, let's reset the watchdog to begin with
            MeadowOS.WatchdogReset();
            // start a thread that pets it
            Thread t = new Thread(() => {
                while (true) {
                    Thread.Sleep(interval);
                    MeadowOS.WatchdogReset();
                }
            });
            t.Start();
        }

        public void RunTest()
        {
            HttpListener listener;
            
            string url = $"http://{Device.WiFiAdapter.IpAddress}/";
            int connectionNumber = 0;

            if (!HttpListener.IsSupported)
            {
                Console.WriteLine("HttpListener class is not supported.");
                return;
            }

            // Create a Http server and start listening for incoming connections
            listener = new HttpListener();
            listener.Prefixes.Add(url);
            listener.Start();
            Console.WriteLine("Listening for connections on {0}", url);

            while (true)
            {
                Console.WriteLine("Getting listener context.");
                HttpListenerContext context = listener.GetContext();

                Console.WriteLine("Getting request from the context.");
                HttpListenerRequest request = context.Request;

                Console.WriteLine("Getting response from the context context.");
                HttpListenerResponse response = context.Response;

                Console.WriteLine("Getting listener context.");

                connectionNumber++;
                string responseString = $"<HTML><BODY><h1>Hello world!</h1><p>Connection {connectionNumber}</p></BODY></HTML>";
                byte[] buffer = System.Text.Encoding.UTF8.GetBytes(responseString);
                response.ContentLength64 = buffer.Length;
                System.IO.Stream output = response.OutputStream;

                Console.WriteLine("Writing response.");
                output.Write(buffer, 0, buffer.Length);

                Console.WriteLine("Closing the output stream.");
                output.Close();
            }

            Console.WriteLine("Getting stopping the listener.");
            listener.Stop();
        }
    }
}
