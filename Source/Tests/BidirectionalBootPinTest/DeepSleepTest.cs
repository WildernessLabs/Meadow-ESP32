using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using System.Threading;

namespace GPIOTest
{
    public class DeepSleepTest : App<F7Micro, SimpleReset>
    {
        private readonly DigitalOutputPort _reset;

        private readonly DigitalOutputPort _boot;

        private void LogMessage(string message)
        {
            Console.WriteLine($"{DateTime.Now.ToString("HH:mm:ss")}: {message}");
        }

        public DeepSleepTest()
        {
            _boot = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);
            _reset = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);
        }
        
        public void Run()
        {
            LogMessage($"{this.ToString()} Testing DeepSleep Wakeup");
            
            LogMessage("Resetting the ESP32.");
            _boot.State = false;
            Thread.Sleep(1);
            _boot.State = true;
            _reset.State = false;
            Thread.Sleep(11);
            _reset.State = true;

            while (true)
            {
                LogMessage("Waking the ESP32.");
                _boot.State = false;
                Thread.Sleep(1);
                _boot.State = true;
                Thread.Sleep(15000);
            }
        }
    }
}
