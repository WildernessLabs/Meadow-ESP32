using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using System.Threading;

namespace GPIOTest
{
    public class BidirectionalBootPinTestApplication : App<F7Micro, BidirectionalBootPinTestApplication>
    {
        /// <summary>
        /// STM32 pin connected to the Reset (EN) pin on the ESP32.
        /// </summary>
        private readonly DigitalOutputPort _reset;

//        private readonly DigitalInputPort _boot;
        private readonly DigitalOutputPort _boot;

        /// <summary>
        /// STM32 pin connected to the boot pin on the ESP32.
        /// </summary>
//        private BiDirectionalPort _boot;

        private void LogMessage(string message)
        {
            Console.WriteLine($"{DateTime.Now.ToString("HH:mm:ss")}: {message}");
        }
        
        /// <summary>
        /// Default constructor.
        /// </summary>
        public BidirectionalBootPinTestApplication()
        {
            _reset = (DigitalOutputPort) Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);

            _boot = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);
//            _boot.Changed += BootOnChanged;
        }

        private void BootOnChanged(object sender, DigitalInputPortEventArgs e)
        {
//            LogMessage($"Boot pin state changed: {_boot.State}");
        }
        
        void ResetESP32()
        {
            LogMessage("Rebooting ESP32.");
//            _boot = (BiDirectionalPort) Device.CreateBiDirectionalPort(Device.Pins.ESP_BOOT, true,
//                resistorMode: ResistorMode.Disabled, initialDirection: PortDirectionType.Input,
//                interruptMode: InterruptMode.EdgeRising);
//            _boot.Direction = PortDirectionType.Output;
//            _boot.State = true;
//            Thread.Sleep(500);
            _reset.State = false;
            Thread.Sleep(10);
            _reset.State = true;
            Console.WriteLine($"Input pin set to {_boot.State}");
//            _boot.Dispose();
//            _boot.Direction = PortDirectionType.Input;
        }

        /// <summary>
        /// Main program loop.
        /// </summary>
        public void Run()
        {
            LogMessage("Rebooting ESP32.");
            _boot.State = true;
            _reset.State = true;
            Thread.Sleep(10);
            _reset.State = false;
            Thread.Sleep(10);
            _reset.State = true;

            while (true)
            {
//                ResetESP32();
//                _boot.State = false;
                Thread.Sleep(10);
                _boot.State = true;
                Thread.Sleep(1000);
            }
        }
    }
}
