using System;
using System.Threading;
using System.Threading.Tasks;

namespace BasicNetworkTest
{
    class Program
    {
        static ScanForNetworksTest application;

        static void Main(string[] args)
        {
            Console.WriteLine("Scan For Access Points");

            application = new ScanForNetworksTest();

            application.RunTest();
            
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
