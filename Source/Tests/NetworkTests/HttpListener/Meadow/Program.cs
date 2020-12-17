using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static HttpListenerTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("HttpListener Test Application");

            application = new HttpListenerTest();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
