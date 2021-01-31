using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static SocketSoakTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("WebSocketPostTestData Test Application");

            application = new SocketSoakTest();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
