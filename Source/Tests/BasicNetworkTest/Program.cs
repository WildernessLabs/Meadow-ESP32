using System;
using System.Threading;

namespace BasicNetworkTest
{
    class MainClass
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("Basic Networking Test Application for Meadow F7");

            BasicNetworkTestApplication application = new BasicNetworkTestApplication();
            application.Run();
        }
    }
}

