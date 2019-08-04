using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using System.Threading;

namespace GPIOTest
{
    public class Esp32ReadyInputPort : App<F7Micro, SimpleReset>
    {
        private readonly DigitalOutputPort _reset;

        private readonly DigitalOutputPort _boot;

        private readonly DigitalInputPort _esp32Ready;

        private void LogMessage(string message)
        {
            Console.WriteLine($"{DateTime.Now.ToString("HH:mm:ss")}: {message}");
        }

        public Esp32ReadyInputPort()
        {
            _boot = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);
            _reset = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);
            _esp32Ready = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX, InterruptMode.None, ResistorMode.Disabled);
        }
        
        public void Run()
        {
            LogMessage($"{this.ToString()} Testing ESP32 Ready Pin as Input Pin");
            LogMessage("Rebooting ESP32.");
            _boot.State = false;
            Thread.Sleep(10);
            _boot.State = true;
            _reset.State = false;
            Thread.Sleep(10);
            _reset.State = true;
            //
            //    Now read the input pin every 250ms.
            //
            while (true)
            {
                LogMessage($"ESP32 Input Pin State: {_esp32Ready.State}");
                Thread.Sleep(100);
            }
        }
    }
}