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
    public class HttpListenerTest : App<F7Micro, HttpListenerTest>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public HttpListenerTest()
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
            HttpListener listener;
            string url = "http://192.168.1.200/";

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
            output.Write(buffer, 0, buffer.Length);

            Console.WriteLine("Closing the output stream.");
            output.Close();

            Console.WriteLine("Getting stopping the listener.");
            listener.Stop();
        }
    }
}
