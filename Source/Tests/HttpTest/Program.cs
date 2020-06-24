using Meadow;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace HttpTest
{
    class Program
    {
        static HttpTest application;

        public static async Task Main(string[] args)
        {
            Console.WriteLine("Network Connection Meadow F7 (Version 0.14.02)");

            application = new HttpTest();

            await application.GetWebPageAsync("http://www.google.com");
            Thread.Sleep(Timeout.Infinite);
        }
    }
}