using System;
using System.Threading;
using Meadow;

namespace MeadowApp
{
    class Program
    {
        // static MeadowApp app;
        public static void Main(string[] args)
        {
            // OauthTest app = new OauthTest();
            // app.RunTest();

            // DeviceInfo app = new DeviceInfo();

            // OutOfMemoryTest app = new OutOfMemoryTest();
            // app.RunAsync().Wait();

            // SoakTest app = new SoakTest();
            // app.RunTest(10);

            WiFiInterfaceControl app = new WiFiInterfaceControl();
            //app.RunTest();
            app.RunAsyncTest().Wait();

            Console.WriteLine("Test complete.");
            Thread.Sleep(Timeout.Infinite);
        }
    }
}