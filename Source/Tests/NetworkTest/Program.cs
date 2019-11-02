using Meadow;
using System;

namespace BasicNetworkTest
{
    class Program
    {
        static IApp application;
        public static void Main(string[] args)
        {
            Console.WriteLine("Basic Networking Test Application for Meadow F7");
            
            application = new NetworkTest();
        }
    }
}
