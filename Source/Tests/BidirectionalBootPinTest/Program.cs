using System;

namespace GPIOTest
{
    class MainClass
    {
        public static void Main(string[] args)
        {
            Console.WriteLine("GPIO Port Test Application for Meadow F7");
            
            Esp32ReadyInputPort application = new Esp32ReadyInputPort();
            
//            BootPinTest application = new BootPinTest(); 
//            DeepSleepTest application = new DeepSleepTest();
//            SimpleReset application = new SimpleReset();
//            BidirectionalBootPinTestApplication application = new BidirectionalBootPinTestApplication();

            application.Run();
        }
    }
}

