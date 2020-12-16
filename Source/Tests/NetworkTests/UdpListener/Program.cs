using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static UdpListenerTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("UdpListener Test Application");

            application = new UdpListenerTest();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
