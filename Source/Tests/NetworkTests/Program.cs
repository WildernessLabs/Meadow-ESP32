using System.Threading;
using Meadow;

namespace MeadowApp
{
    class Program
    {
        // static MeadowApp app;
        public static void Main(string[] args)
        {
            OauthTest app = new OauthTest();
            app.RunTest();

            // DeviceInfo app = new DeviceInfo();

            Thread.Sleep(Timeout.Infinite);
        }
    }
}
