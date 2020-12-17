using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static TcpListenerTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("TcpListener Test Application");

            application = new TcpListenerTest();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
