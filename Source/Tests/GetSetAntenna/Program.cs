using System;

namespace GetSetAntenna
{
    class Program
    {
        static GetSetAntennaTest application;
        public static void Main(string[] args)
        {
            Console.WriteLine("GetAntenna Test Application.");
            
            application = new GetSetAntennaTest();
            application.RunTest();
        }
    }
}
