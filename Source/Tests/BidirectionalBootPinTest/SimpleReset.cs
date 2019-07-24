using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using System.Threading;

namespace GPIOTest
{
    public class SimpleReset : App<F7Micro, SimpleReset>
    {
        private readonly DigitalOutputPort _reset;

        private readonly DigitalOutputPort _boot;

        private void LogMessage(string message)
        {
            Console.WriteLine($"{DateTime.Now.ToString("HH:mm:ss")}: {message}");
        }

        public SimpleReset()
        {
            _boot = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);
            _reset = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);
        }
        
        public void Run()
        {
            LogMessage($"{this.ToString()} Testing Reset");
            while (true)
            {
                LogMessage("Rebooting ESP32.");
                _boot.State = false;
                Thread.Sleep(10);
                _boot.State = true;
                _reset.State = false;
                Thread.Sleep(10);
                _reset.State = true;
                Thread.Sleep(25000);
            }
        }
    }
}
