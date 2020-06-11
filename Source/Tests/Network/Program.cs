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
            Console.WriteLine("Network Connection Meadow F7 (Version 0.14.01)");

            application = new NetworkTest();
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
