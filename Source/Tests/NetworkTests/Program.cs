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
            //OauthTest app = new OauthTest();
            //app.RunTest().Wait();

            DeviceInfo app = new DeviceInfo();
            app.RunTest();

            // OutOfMemoryTest app = new OutOfMemoryTest();
            // app.RunAsync().Wait();

            //DisconnectTest app = new DisconnectTest();
            //app.RunTest().Wait();

            //SoakTest app = new SoakTest();
            //app.RunTest(10).Wait();

            //WiFiInterfaceControl app = new WiFiInterfaceControl();
            //app.RunTest();
            //app.RunAsyncTest().Wait();

            // EasySocketTest app = new EasySocketTest();
            // app.RunTest();

            // HttpsTest app = new HttpsTest();
            // app.RunAsyncTest().Wait();

            // DnsTest app = new DnsTest();
            // app.RunAsync().Wait();

            // NtpTest app = new NtpTest();
            // app.RunAsync().Wait();

            Console.WriteLine("Test complete.");
            Thread.Sleep(Timeout.Infinite);
        }
    }
}