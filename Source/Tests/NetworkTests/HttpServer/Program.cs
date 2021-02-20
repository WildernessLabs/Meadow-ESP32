using System;
using System.Threading;
using System.Threading.Tasks;

namespace MeadowApp
{
    class Program
    {
        static HttpServerTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("HttpListener Test Application");

            application = new HttpServerTest();
            
            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
