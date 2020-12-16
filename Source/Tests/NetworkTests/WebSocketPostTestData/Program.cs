using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static WebSocketPostTestDataTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("WebSocketPostTestData Test Application");

            application = new WebSocketPostTestDataTest();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
