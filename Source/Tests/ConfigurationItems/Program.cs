using Meadow;
using System;

namespace BasicNetworkTest
{
    class Program
    {
        static ConfigurationItemsTest application;
        public static void Main(string[] args)
        {
            Console.WriteLine("Checking configuration items for the ESP32.");
            
            application = new ConfigurationItemsTest();
            application.RunTest();
        }
    }
}
