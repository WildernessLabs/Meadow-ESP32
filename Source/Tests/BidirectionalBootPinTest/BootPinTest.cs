using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using System.Threading;

namespace GPIOTest
{
    public class BootPinTest : App<F7Micro, SimpleReset>
    {
        private readonly DigitalOutputPort _reset;

        private DigitalOutputPort _bootOutput;

        private DigitalInputPort _bootInput;

        private DigitalOutputPort _d04Output;

        private DigitalInputPort _d04Input;

        private void LogMessage(string message)
        {
            Console.WriteLine($"{DateTime.Now.ToString("HH:mm:ss")}: {message}");
        }

        public BootPinTest()
        {
            _reset = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);
        }
        
        public void Run()
        {
            LogMessage($"{this.ToString()} Testing DeepSleep Wakeup");
            
            LogMessage("Resetting the ESP32.");
            _bootOutput = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);
            _bootOutput.State = false;
            Thread.Sleep(1);
            _bootOutput.State = true;
            _reset.State = false;
            Thread.Sleep(11);
            _reset.State = true;
            _bootOutput.Dispose();

            _bootInput = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_BOOT, resistorMode: ResistorMode.Disabled);
            _d04Input = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.D04, resistorMode: ResistorMode.Disabled);

            while (true)
            {
                LogMessage("Waking the ESP32.");
                _bootInput?.Dispose();
                _d04Input?.Dispose();
                
                _bootOutput = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, false);
                _d04Output = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.D04, false);
                Thread.Sleep(5);
                _bootOutput.State = true;
                _d04Output.State = true;
                Thread.Sleep(5);
                
                _bootOutput.Dispose();
                _d04Output.Dispose();
                _bootInput = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_BOOT, resistorMode: ResistorMode.Disabled);
                _d04Input = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.D04, resistorMode: ResistorMode.Disabled);

                Thread.Sleep(500);
            }
        }
    }
}
