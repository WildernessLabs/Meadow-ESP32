using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static BatteryLevelTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("BatteryLevel Test Application");

            application = new BatteryLevelTest();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
