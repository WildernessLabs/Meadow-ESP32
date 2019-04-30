using System;
using System.Threading;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Hardware.Communications;
using Meadow.Core.Esp32;

namespace Meadow.Core.Esp32
{
    public class SpiTransportProvider : ITransportProvider
    {
        #region Enums

        /// <summary>
        /// Possible SPI states.
        /// </summary>
        private enum SpiStates
        {
            Invalid, Idle, AcknowledgingMessageHeader,
            WaitingForMessageBody, AcknowledingMessageBody,
            WantToSendMessageHeader, WaitingForMessageHeaderAcknowledgement,
            WantToSendMessageBody, WaitingForMessageBodyAcknowledgement,
            WantToSendTransportMessage
        };

        #endregion

        #region Class level variables.

        /// <summary>
        /// Pin used by the STM32 that it is ready.
        /// </summary>
        private readonly DigitalOutputPort _stm32ReadyPin;

        /// <summary>
        /// Pin used by the ESP32 to indicate that it is ready to send or receive data.
        /// </summary>
        private readonly DigitalInputPort _esp32ReadyPin;

        /// <summary>
        /// SPI interface used to communicate with the ESP32.
        /// </summary>
        private readonly SoftwareSPIBus _esp32;

        private readonly DigitalOutputPort _reset;
        private readonly DigitalOutputPort _boot;

        /// <summary>
        /// Current state of the SPI interface.
        /// </summary>
        /// <remarks>
        /// The class uses a sequence of interrupts to signal that changes in the
        /// state and what action is expected next.  This mirrors the c code on the ESP32
        /// in order to make it obvious where complementary changes are required.
        /// </remarks>
        SpiStates _currentState = SpiStates.Invalid;

        /// <summary>
        ///     Queue of messages to be sent to the ESP32.
        /// </summary>
        private static Queue<Messages.Message> _messageQueue = null;

        /// <summary>
        ///     Lock for the message queue to synchronise access by the
        ///     two threads (main application adding messages and the
        ///     queue processing thread taking messages from the queue).
        /// </summary>
        private object _messageQueueLock = new object();

        /// <summary>
        ///     Handle used to synchronise the incoming messages and the task processing the messages.
        /// </summary>
        private EventWaitHandle _waitingForStuffToDoHandle = new AutoResetEvent(false);

        /// <summary>
        ///     Handle used to synchronise the responses from the Uart.
        /// </summary>
        private EventWaitHandle _waitingForResponseHandle = new AutoResetEvent(false);

        /// <summary>
        ///     Thread processing the messages as they enter the queue.
        /// </summary>
        private Thread _queueHandler = null;

        /// <summary>
        /// 
        /// </summary>
        private Messages.Message? _inboundMessage = null;

        /// <summary>
        /// 
        /// </summary>
        private Messages.Message? _outboundMessage = null;

        /// <summary>
        /// This value is used a lot so make it available as a class level variable.
        /// </summary>
        private UInt32 _headerFrameSize = Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE);

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
            _stm32ReadyPin = (DigitalOutputPort)device.CreateDigitalOutputPort(stm32ReadyPin, false);
            _esp32ReadyPin = (DigitalInputPort)device.CreateDigitalInputPort(esp32ReadyPin);
            _esp32ReadyPin.Changed += _esp32ReadyPin_Changed;
            _esp32 = new SoftwareSPIBus(device, mosi, miso, clock, chipSelect);
            _reset = (DigitalOutputPort)device.CreateDigitalOutputPort(resetPin, true);
            _boot = (DigitalOutputPort)device.CreateDigitalOutputPort(bootPin, true);
            _currentState = SpiStates.Idle;
            _messageQueue = new Queue<Messages.Message>();
            _queueHandler = new Thread(() =>
            {
                Run();
            });
            _queueHandler.Start();
        }

        #endregion

        #region Delegates and events

        /// <summary>
        ///     Error event to allow lower level errors to bubble up to the interfaces.
        /// </summary>
        public event MessageDispatcherErrorEventHandler TransportError = delegate { };

        /// <summary>
        ///     Data received event.
        /// </summary>
        public event TransportProviderDataReceivedEventHandler MessageReceived = delegate { };

        /// <summary>
        /// The ESP32 is indicating that it is ready to process some data from the STM32
        /// or that it has data ready to send to the STM32.  The exact action is worked out
        /// by the Run method, this event simply sends the signal that there is stuff to do.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _esp32ReadyPin_Changed(object sender, DigitalInputPortEventArgs e)
        {
            _waitingForStuffToDoHandle.Set();
        }

        #endregion

        #region Methods

        private void Run()
        {
            while (true)
            {
                _waitingForStuffToDoHandle.WaitOne();
                switch (_currentState)
                {
                    case SpiStates.Idle:
                        ProcessIdleState();
                        break;
                    case SpiStates.AcknowledgingMessageHeader:
                        ProcessAcknowledgingMessageHeaderState();
                        break;
                    case SpiStates.WaitingForMessageBody:
                        ProcessWaitingForMessageBodyState();
                        break;
                    case SpiStates.AcknowledingMessageBody:
                        ProcessAcknowledingMessageBodyState();
                        break;
                    case SpiStates.WantToSendMessageHeader:
                        ProcessWantToSendMessageHeaderState();
                        break;
                    case SpiStates.WaitingForMessageHeaderAcknowledgement:
                        ProcessWaitingForMessageHeaderAcknowledgementState();
                        break;
                    case SpiStates.WantToSendMessageBody:
                        ProcessWantToSendMessageBodyState();
                        break;
                    case SpiStates.WaitingForMessageBodyAcknowledgement:
                        ProcessWaitingForMessageBodyAcknowledgementState();
                        break;
                    case SpiStates.WantToSendTransportMessage:
                        ProcessWantToSendTransportMessageState();
                        break;
                    default:
                        break;
                }
            }
        }

        /// <summary>
        /// Process the Idle state when we have recevied a signal that there is some work to do.
        /// </summary>
        /// <remarks>
        /// This method really works out what to do as there are two possible scenarios:
        ///     - A message has arrived in the queue and the method taking the message has
        ///       indicated so
        ///     - The ESP32 has raised a ready signal to take the next message or to indicate
        ///       that it is ready to process data.
        ///
        /// The algorithm here is simply that outbound messages take priority over inbound
        /// messages (for the moment).
        /// </remarks>
        private void ProcessIdleState()
        {
            lock (_messageQueueLock)
            {
                if (_messageQueue.Count > 0)
                {
                    _outboundMessage = _messageQueue.Dequeue();
                    _currentState = SpiStates.WantToSendMessageHeader;
                    _stm32ReadyPin.State = true;
                }
                else
                {

                }
            }
        }

        private void ProcessAcknowledgingMessageHeaderState()
        {
        }

        private void ProcessWaitingForMessageBodyState()
        {
        }

        private void ProcessAcknowledingMessageBodyState()
        {
        }

        /// <summary>
        /// ESP32 has indicated that it is ready so send the message header.
        /// </summary>
        private void ProcessWantToSendMessageHeaderState()
        {
            byte[] encodedHeader = Encoders.EncodeHeader((Messages.Message)_outboundMessage);
            _esp32.WriteBytes(encodedHeader);
            _currentState = SpiStates.WaitingForMessageHeaderAcknowledgement;
            _stm32ReadyPin.State = false;
        }

        /// <summary>
        /// 
        /// </summary>
        private void ProcessWaitingForMessageHeaderAcknowledgementState()
        {
            byte[] dummyHeader = new byte[_headerFrameSize];
            Array.Clear(dummyHeader, 0, (int) _headerFrameSize);
            byte[] encodedAcknowledgement = _esp32.WriteRead(dummyHeader, (ushort)_headerFrameSize);
            Messages.Message ackknowledgement = Encoders.ExtractMessage(encodedAcknowledgement);
            //
            //  Now need to work out what to do, was there an error?
            //
            if (_outboundMessage.Value.PayloadLength > 0)
            {
                _currentState = SpiStates.WantToSendMessageBody;
            }
            _stm32ReadyPin.State = false;
        }

        private void ProcessWantToSendMessageBodyState()
        {
        }

        private void ProcessWaitingForMessageBodyAcknowledgementState()
        {
        }

        private void ProcessWantToSendTransportMessageState()
        {
        }

        /// <summary>
        /// Queue a for sending to the ESP32.
        /// </summary>
        /// <param name="message">Message to be queued.</param>
        public void QueueMessageForEsp32(Messages.Message message)
        {
            lock (_messageQueueLock)
            {
                _messageQueue.Enqueue(message);
            }
            //
            //  Inform the task processing the messages that a new message
            //  has been queued so there is at least one message waiting to
            //  be sent.
            //
            _waitingForStuffToDoHandle.Set();
        }

        #endregion
    }
}
