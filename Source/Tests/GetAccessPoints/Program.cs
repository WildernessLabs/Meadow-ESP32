using Meadow;
using System;

namespace BasicNetworkTest
{
    class Program
    {
        static IApp application;
        public static void Main(string[] args)
        {
            Console.WriteLine("GetAccessPoint Test Application for Meadow F7 (Version 0.12.01)");
            
            application = new GetSetAccessPointsTest();
        }
    }
}
