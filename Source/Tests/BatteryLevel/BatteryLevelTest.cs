using System;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway;

namespace MeadowApp
{
    public class BatteryLevelTest : App<F7Micro, BatteryLevelTest>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public BatteryLevelTest()
        {
        }

        public void RunTest()
        {
            while (true)
            {
                Console.WriteLine($"Battery level: {Device.Coprocessor.BatteryLevel}");
                Thread.Sleep(1000);
            }
        }
    }
}
