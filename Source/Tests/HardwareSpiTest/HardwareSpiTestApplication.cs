using System;
using System.Text;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Core;
using Meadow.Hardware;
using DebugHelpers;

namespace HardwareSpiTest
{
    public class HardwareSpiTestApplication : App<F7Micro, HardwareSpiTestApplication>
    {
        public HardwareSpiTestApplication()
        {
            //
            //  Reset the ESP32 chip.
            //
            DebugHelpers.DebugHelpers.LogMessage("Resetting the ESP32.");
            DigitalOutputPort boot = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_BOOT, true);
            boot.State = true;
            DigitalOutputPort reset = (DigitalOutputPort)Device.CreateDigitalOutputPort(Device.Pins.ESP_RST, true);
            reset.State = false;
            Thread.Sleep(20);
            reset.State = true;

            //IDigitalOutputPort chipSelect = Device.CreateDigitalOutputPort(Device.Pins.D04, true);
            //ISpiBus bus = Device.CreateSpiBus(Device.Pins.SCK, Device.Pins.MOSI, Device.Pins.MISO);

            IDigitalOutputPort chipSelect = Device.CreateDigitalOutputPort(Device.Pins.ESP_CS, true);
            SpiClockConfiguration spiConfig = new SpiClockConfiguration(350, SpiClockConfiguration.Mode.Mode3);
            ISpiBus bus = Device.CreateSpiBus(Device.Pins.ESP_CLK, Device.Pins.ESP_MOSI, Device.Pins.ESP_MISO, spiConfig);

            DebugHelpers.DebugHelpers.LogMessage("Supported speeds:");
            foreach (long speed in bus.SupportedSpeeds)
            {
                DebugHelpers.DebugHelpers.LogMessage($"    {speed}");
            }

            DebugHelpers.DebugHelpers.LogMessage("Creating SPI bus.");
            //SpiPeripheral esp32 = new SpiPeripheral(bus, chipSelect);

            IDigitalInputPort rx = (IDigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.ESP_UART_RX, resistorMode: ResistorMode.Disabled);
            IDigitalInputPort tx = (IDigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX, resistorMode: ResistorMode.Disabled);

            int counter = 0;
            while (true)
            {
                string message = $"Message number {counter++}";
                byte[] data = Encoding.UTF8.GetBytes(message);
                int length = data.Length + 1;
                if ((length % 4) != 0)
                {
                    length = length + 4 - (length % 4);
                }
                length = 48;
                byte[] dataPacket = new byte[length];
                byte[] readData = new byte[length];
                Array.Clear(dataPacket, 0, length);
                Array.Copy(data, dataPacket, data.Length);

                DebugHelpers.DebugHelpers.LogMessage("****************************************************************************************************");
                DebugHelpers.DebugHelpers.LogMessage(message);
                DebugHelpers.DebugHelpers.LogMessage("Sending buffer:");
                DebugHelpers.DebugHelpers.LogMessage(DebugHelpers.DebugHelpers.PrintableBuffer(dataPacket));

                bus.ExchangeData(chipSelect, ChipSelectMode.ActiveLow, dataPacket, readData);

                DebugHelpers.DebugHelpers.LogMessage("Received:");
                DebugHelpers.DebugHelpers.LogMessage(DebugHelpers.DebugHelpers.PrintableBuffer(readData));
                DebugHelpers.DebugHelpers.LogMessage("");
                Thread.Sleep(500);
            }
        }
    }
}
