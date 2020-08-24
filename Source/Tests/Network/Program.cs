using System;
using System.Threading;
using System.Threading.Tasks;

namespace BasicNetworkTest
{
    class Program
    {
        static NetworkTest application;

        public static async Task Main(string[] args)
        {
            Console.WriteLine("Network Connection Meadow F7 (Version 0.14.06)");

            application = new NetworkTest();

            //NetworkInterface[] adapters = NetworkInterface.GetAllNetworkInterfaces();

            //Console.WriteLine($"{adapters.Length} network adapters found.");
            //if (adapters.Length > 0)
            //{
            //    Console.WriteLine($"Adapter description: {adapters[0].Description}");
            //}

            //await application.GetWebPageAsync("http://www.google.com");

            //application.PostData("http://io.adafruit.com/api/v2/nevyn/feeds/airtemperature/data", 23.2F);


            //await application.PostDataAsync("http://io.adafruit.com/api/v2/nevyn/feeds/airtemperature/data");
            //Thread.Sleep(Timeout.Infinite);

            // application.WebSocketPost();

            application.ScanForAccessPoints();

            //application.StartListening();
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
