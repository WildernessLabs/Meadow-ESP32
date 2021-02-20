using System;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway;
using System.Text;

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
            StringBuilder message = new StringBuilder();
            while (true)
            {
                message.Clear();
                message.Append(DateTime.Now.ToString("U"));
                message.Append($" - Battery level: {Device.Coprocessor.BatteryLevel}");
                Console.WriteLine(message.ToString());
                Thread.Sleep(1000);
            }
        }
    }
}
