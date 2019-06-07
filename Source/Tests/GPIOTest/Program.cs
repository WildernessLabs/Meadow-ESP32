using System;

namespace GPIOTest
{
    class MainClass
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("GPIO Test Application for Meadow F7");

            GPIOTestApplication application = new GPIOTestApplication();
            application.Run();
        }
    }
}

