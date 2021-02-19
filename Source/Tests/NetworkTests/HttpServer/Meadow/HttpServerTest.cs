using System;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    public class HttpServerTest : App<F7Micro, HttpServerTest>
    {
        public static HttpListener listener;
        public static string url = "http://192.168.1.211:80/";
        public static int pageViews = 0;
        public static int requestCount = 0;
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

        /// <summary>
        /// Default constructor.
        /// </summary>
        public HttpServerTest()
        {
            Console.WriteLine("Creating HttpListenerTest object.");
            MeadowOS.WatchdogEnable(25000);
            StartWatchdog(20000);
            Device.InitWiFiAdapter().Wait();
            Device.WiFiAdapter.WiFiConnected += WiFiAdapter_WiFiConnected;
            Console.WriteLine("Connecting to access point.");
            Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).Wait();
            Console.WriteLine("WiFi connection completed.");
        }

        private void WiFiAdapter_WiFiConnected(object sender, EventArgs e)
        {
            Console.WriteLine("WiFiAdapter_WiFiConnected: connected to access point.");
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

        public void RunTest()
        {
            listener = new HttpListener();
            listener.Prefixes.Add(url);
            listener.Start();
            Console.WriteLine("Listening for connections on {0}", url);

            // Handle requests
            Task listenTask = HandleIncomingConnections();
            listenTask.GetAwaiter().GetResult();

            // Close the listener
            listener.Close();
        }
    }
}
