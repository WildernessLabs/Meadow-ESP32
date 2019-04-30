using System;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Hardware.Communications;
using Meadow.Core.Esp32;


namespace Meadow.Core.Esp32
{
    public class SpiTransportProvider : ITransportProvider
    {
        #region Properties

        private readonly DigitalOutputPort _stm32ReadyPin;
        private readonly DigitalInputPort _esp32ReadyPin;
        private readonly SoftwareSPIBus _esp32;
        private readonly DigitalOutputPort _reset;
        private readonly DigitalOutputPort _boot;

        #endregion

        #region Constructor(s)

        /// <summary>
        ///     Default constructor is private to prevent it from being called.
        /// </summary>
        private SpiTransportProvider()
        {
        }

        /// <summary>
        /// Default constructor for the SPI Transport Provider implementation.
        /// </summary>
        public SpiTransportProvider(IIODevice device, IPin stm32ReadyPin, IPin esp32ReadyPin, IPin mosi, IPin miso, IPin clock, IPin chipSelect, IPin bootPin, IPin resetPin)
        {
            _stm32ReadyPin = (DigitalOutputPort) device.CreateDigitalOutputPort(stm32ReadyPin, false);
            _esp32ReadyPin = (DigitalInputPort) device.CreateDigitalInputPort(esp32ReadyPin);
            _esp32 = new SoftwareSPIBus(device, mosi, miso, clock, chipSelect);
            _reset = (DigitalOutputPort) device.CreateDigitalOutputPort(resetPin, true);
            _boot = (DigitalOutputPort) device.CreateDigitalOutputPort(bootPin, true);
        }

        #endregion

        #region Deletgaes and events

        /// <summary>
        ///     Error event to allow lower level errors to bubble up to the interfaces.
        /// </summary>
        public event MessageDispatcherErrorEventHandler TransportError = delegate { };

        /// <summary>
        ///     Data received event.
        /// </summary>
        public event TransportProviderDataReceivedEventHandler DataReceived = delegate { };

        #endregion

        #region Methods

        /// <summary>
        /// Queue a for sending to the ESP32.
        /// </summary>
        /// <param name="message">Message to be queued.</param>
        public void QueueMessageForEsp32(Messages.Message message)
        {

        }

        #endregion
    }
}
