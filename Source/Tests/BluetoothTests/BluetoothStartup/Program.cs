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

            var i = 0;

            while (true)
            {
                app.SetIntValue(i++);
                Console.WriteLine($"{i}");
                Thread.Sleep(2000);
            }
        }
    }
}