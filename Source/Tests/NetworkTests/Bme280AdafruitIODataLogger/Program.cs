using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static Bme280AdafruitIODataLogger application;

        static void Main(string[] args)
        {
            Console.WriteLine("BME280 Adafruit IO Data Logger");

            application = new Bme280AdafruitIODataLogger();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
