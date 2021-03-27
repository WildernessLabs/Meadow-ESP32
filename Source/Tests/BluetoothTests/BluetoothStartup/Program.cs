using System;
using System.Threading;
using Meadow;

namespace MeadowApp
{
    class Program
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("=== BLUETOOTH STARTUP TEST APP ===");
            // instantiate and run new meadow app
            var app = new BluetoothStartupTest();
            app.Startup();

            Thread.Sleep(Timeout.Infinite);
        }
    }
}