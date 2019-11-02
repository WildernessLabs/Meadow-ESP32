using System;
using Meadow;

namespace HardwareSpiTest
{
    class MainClass
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("Hardware SPI test for STM -> ESP32 Communication.");

            IApp application = new HardwareSpiTestApplication();
        }
    }
}

