using Meadow;
using System;
using System.Threading;

namespace BasicNetworkTest
{
    class Program
    {
        static IApp application;
        public static void Main(string[] args)
        {
            Console.WriteLine("HttpGet Test Application for Meadow F7 (Version 0.12.01)");
            
            application = new HttpGetTest();
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
