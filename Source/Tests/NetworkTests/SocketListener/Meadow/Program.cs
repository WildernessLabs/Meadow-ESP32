using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static SocketListenerTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("HttpListener Test Application");

            application = new SocketListenerTest();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
