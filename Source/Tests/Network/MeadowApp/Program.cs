using System;
using System.Threading;
using Meadow;

namespace MeadowApp
{
    class Program
    {
        public static void Main(string[] args)
        {
            TestRunner app = new TestRunner();
            app.Run();

            Console.WriteLine("Tests complete.");
            Thread.Sleep(Timeout.Infinite);
        }
    }
}