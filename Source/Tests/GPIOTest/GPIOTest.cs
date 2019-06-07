using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using System.Threading;

namespace GPIOTest
{
    public class GPIOTestApplication : App<F7Micro, GPIOTestApplication>
    {
        private DigitalInputPort _button;

        /// <summary>
        /// Pin used by the STM32 that it is ready.
        /// </summary>
        private readonly DigitalOutputPort _stm32ReadyPin;

        /// <summary>
        /// Pin used by the ESP32 to indicate that it is ready to send or receive data.
        /// </summary>
        private DigitalInputPort _esp32ReadyPin;

        /// <summary>
        /// STM32 pin connected to the Reset (EN) pin on the ESP32.
        /// </summary>
        private readonly DigitalOutputPort _reset;

        /// <summary>
        /// STM32 pin connected to the boot pin on the ESP32.
        /// </summary>
        private readonly BiDirectionalPort _boot;
        
        private readonly DigitalInputPort _esp32Ready;

        private readonly DigitalInputPort _esp32TxPin;

        /// <summary>
        /// Default constructor.
        /// </summary>
        public GPIOTestApplication()
        {
//            _button = (DigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.D14, interruptMode: InterruptMode.EdgeFalling, resistorMode: ResistorMode.PullUp, debounceDuration: 100);
//            _button.Changed += _button_Changed;

            _reset = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, false);

            _esp32ReadyPin = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.ESP_UART_RX, InterruptMode.EdgeRising, ResistorMode.Disabled);
            _esp32ReadyPin.Changed += Esp32ReadyOnChanged;

            _stm32ReadyPin = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);
            
            _esp32TxPin = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX, InterruptMode.None, ResistorMode.Disabled);
        }

        private void Esp32ReadyOnChanged(object sender, DigitalInputPortEventArgs e)
        {
            _stm32ReadyPin.State = !_stm32ReadyPin.State;
            _stm32ReadyPin.State = !_stm32ReadyPin.State;
            Console.WriteLine($"{DateTime.Now.ToString("HH:mm:ss")} ESP32 is ready.");
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _button_Changed(object sender, DigitalInputPortEventArgs e)
        {
            Console.WriteLine("Toggling GPIO"); 
        }

        void ResetESP32()
        {
            Console.WriteLine("Rebooting ESP32.");
            
//            _esp32ReadyPin?.Dispose();
//            _esp32ReadyPin = null;
//
//            DigitalOutputPort boot = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, false);
//            _reset.State = true;
//            _reset.State = false;
//            Thread.Sleep(250);
//            _reset.State = true;
//            
//            boot.Dispose();
//            boot = null;
//            
//            SetupEsp32ReadyPin();

            _stm32ReadyPin.State = true;
            _reset.State = false;
            Thread.Sleep(200);
            _reset.State = true;
            _stm32ReadyPin.State = false;
            
            Console.WriteLine("Reboot complete.");
        }

        /// <summary>
        /// Main program loop.
        /// </summary>
        public void Run()
        {
            Console.WriteLine("Basic GPIO Communication Test.");
            Console.WriteLine();
            ResetESP32();
            Console.WriteLine("Preparing to pulse ready pin.");
            Thread.Sleep(500);
            _stm32ReadyPin.State = true;
            
            Console.WriteLine("Initialisation complete, waiting for interrupts.");
            Thread.Sleep(Timeout.Infinite);
        }
    }
}
