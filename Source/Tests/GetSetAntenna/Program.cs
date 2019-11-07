using Meadow;
using System;

namespace BasicNetworkTest
{
    class Program
    {
        static IApp application;
        public static void Main(string[] args)
        {
            Console.WriteLine("GetAntenna Test Application for Meadow F7 (Version 0.12.01)");
            
            application = new GetSetAntennaTest();
        }
    }
}
