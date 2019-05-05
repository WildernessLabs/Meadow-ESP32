using System;
using System.Threading;
using System.Collections.Generic;
using Meadow.Hardware;
using Meadow.Hardware.Communications;

namespace Meadow.Core.Esp32
{
    public class SpiTransportProvider : TransportProviderBase
    {
        #region Enums

        /// <summary>
        /// Possible SPI states.
        /// </summary>
        private enum SpiStates
        {
            Invalid, Idle, AcknowledgingMessageHeader,
            WaitingForMessageBody, AcknowledgingMessageBody,
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

        /// <summary>
        /// STM32 pin connected to the Reset (EN) pin on the ESP32.
        /// </summary>
        private readonly DigitalOutputPort _reset;

        /// <summary>
        /// STM32 pin connected to the boot pin on the ESP32.
        /// </summary>
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
        private readonly object _messageQueueLock = new object();

        /// <summary>
        ///     Handle used to synchronise the incoming messages and the task processing the messages.
        /// </summary>
        private readonly EventWaitHandle _waitingForStuffToDoHandle = new AutoResetEvent(false);

        /// <summary>
        ///     Thread processing the messages as they enter the queue.
        /// </summary>
        private Thread _queueHandler = null;

        /// <summary>
        /// 
        /// </summary>
        private Messages.Message? _inboundMessage = null;

        /// <summary>
        /// Number of times we have retried to send this message.
        /// </summary>
        private int _currentRetryCount;

        /// <summary>
        /// Last status code for an incoming message operation.
        /// </summary>
        /// <remarks>
        /// This variable is used to transfer status codes between the methods used
        /// for sending and receiving data.
        /// </remarks>
        private StatusCodes _lastStatusCode = StatusCodes.CompletedOK;

        /// <summary>
        /// Current message being sent by the system.
        /// </summary>
        /// <remarks>
        /// This needs to be a class level variable as it is processed by a
        /// state machine which is implemented by a number of different methods.
        /// </remarks>
        private Messages.Message? _outboundMessage = null;

        /// <summary>
        /// This value is used a lot so make it available as a class level variable.
        /// </summary>
        private readonly UInt32 _headerFrameSize = Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE);

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
            _esp32ReadyPin = (DigitalInputPort) device.CreateDigitalInputPort(esp32ReadyPin, interruptMode: InterruptMode.EdgeRising);
            _esp32ReadyPin.Changed += _esp32ReadyPin_Changed;
            _esp32 = new SoftwareSPIBus(device, mosi, miso, clock, chipSelect);
            _reset = (DigitalOutputPort) device.CreateDigitalOutputPort(resetPin, true);
            _boot = (DigitalOutputPort) device.CreateDigitalOutputPort(bootPin, true);

            _boot.State = true;
            _reset.State = false;
            _reset.State = true;

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
        public override event TransportProviderErrorEventHandler TransportError = delegate { };

        /// <summary>
        ///     Data received event.
        /// </summary>
        public override event TransportProviderDataReceivedEventHandler MessageReceived = delegate { };

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

        /// <summary>
        /// Main MessageDispatcher thread.
        /// </summary>
        private void Run()
        {
            while (true)
            {
                //
                //  Work out if we should wait for an incoming signal.
                //
                //  If we just wait then the application will process only the first
                //  message in the queue as only one signal will be processed even if
                //  more than one signals are raised.
                //
                bool wait = true;
                if (_currentState == SpiStates.Idle)
                {
                    lock (_messageQueueLock)
                    {
                        wait = (_messageQueue.Count == 0);
                    }
                }
                if (wait)
                {
                    _waitingForStuffToDoHandle.WaitOne();
                }
                //
                //  Signals are used to transition from one state to another.
                //
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
                    case SpiStates.AcknowledgingMessageBody:
                        ProcessAcknowledgingMessageBodyState();
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
        /// Process the Idle state when we have received a signal that there is some work to do.
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
            Console.WriteLine("Entering ProcessIdleState");
            lock (_messageQueueLock)
            {
                if (_messageQueue.Count > 0)
                {
                    _outboundMessage = _messageQueue.Dequeue();
                    _currentState = SpiStates.WantToSendMessageHeader;
                    _currentRetryCount = 0;
                    _stm32ReadyPin.State = true;
                }
                else
                {
                    Messages.Message readyMessage = new Messages.Message(PacketType.Ready, Esp32Interfaces.None, Messages.INVALID_FUNCTION, Messages.INVALID_MESSAGE_ID, 0, null);
                    byte[] encodedReadyMessage = Encoders.EncodeMessage(readyMessage, true);

                    _stm32ReadyPin.State = true;
                    byte[] encodedIncomingHeader = _esp32.WriteRead(encodedReadyMessage, (ushort) encodedReadyMessage.Length);
                    (_inboundMessage, _lastStatusCode) = Encoders.ExtractMessage(encodedIncomingHeader);
                    _currentState = SpiStates.WaitingForMessageHeaderAcknowledgement;
                    _stm32ReadyPin.State = false;
                }
            }
        }

        /// <summary>
        /// The message header was sent by the ESP32 at the same time the STM32
        /// sent the Ready packet.  Check to see if we have a valid header message.
        /// </summary>
        private void ProcessAcknowledgingMessageHeaderState()
        {
            Console.WriteLine("Entering ProcessAcknowledgingMessageHeaderState ");
            Messages.Message acknowledgementMessage;
            if (_lastStatusCode != StatusCodes.CompletedOK)
            {
                acknowledgementMessage = new Messages.Message(PacketType.Nak, Esp32Interfaces.Transport, 0, 0, (UInt32) _lastStatusCode, null);
            }
            else
            {
                Messages.Message inbound = _inboundMessage.Value;
                acknowledgementMessage = new Messages.Message(PacketType.Ack, (Esp32Interfaces) inbound.Interface, inbound.Function, inbound.MessageID, (UInt32) StatusCodes.CompletedOK, null);
            }
            byte[] encodedAcknowledgementMessage = Encoders.EncodeMessage(acknowledgementMessage);
            _esp32.WriteBytes(encodedAcknowledgementMessage);
            if (_lastStatusCode != StatusCodes.CompletedOK)
            {
                _lastStatusCode = StatusCodes.CompletedOK;
                _inboundMessage = null;
                _currentState = SpiStates.Idle;
            }
            else
            {
                if (_inboundMessage.Value.PayloadLength > 0)
                {
                    _currentState = SpiStates.WaitingForMessageBody;
                }
                else
                {
                    MessageReceviedEventArgs args = new MessageReceviedEventArgs(_inboundMessage.Value);
                    MessageReceived(this, args);
                    _currentState = SpiStates.Idle;
                }
            }
            _stm32ReadyPin.State = false;
            if (_currentState == SpiStates.WantToSendMessageBody)
            {
                _stm32ReadyPin.State = true;
            }
        }

        /// <summary>
        /// Process an inbound message body.
        /// </summary>
        private void ProcessWaitingForMessageBodyState()
        {
            Console.WriteLine("Entering ProcessWaitingForMessageBodyState");
            ushort incomingMessageLength = (ushort) (Messages.HEADER_SIZE + _inboundMessage.Value.PayloadLength);
            byte[] encodedDummyMessage = new byte[incomingMessageLength];
            byte[] encodedInboundMessage = _esp32.WriteRead(encodedDummyMessage, incomingMessageLength);
            StatusCodes status;
            Messages.Message? inboundMessage;
            (inboundMessage, status) = Encoders.ExtractMessage(encodedInboundMessage);
            if (status != StatusCodes.CompletedOK)
            {
                _lastStatusCode = status;
            }
            else
            {
                if ((inboundMessage.Value.Interface == _inboundMessage.Value.Interface) && (inboundMessage.Value.MessageID == _inboundMessage.Value.MessageID))
                {
                    _lastStatusCode = StatusCodes.HeaderBodyFieldMismatch;
                }
                else
                {
                    _inboundMessage = inboundMessage;
                    _lastStatusCode = StatusCodes.CompletedOK;
                }
            }
            _currentState = SpiStates.AcknowledgingMessageBody;
            _stm32ReadyPin.State = false;
            _stm32ReadyPin.State = true;
        }

        /// <summary>
        /// Acknowledge the incoming message body.
        /// </summary>
        /// <remarks>
        /// If the message transmission reception is successful then the incoming message
        /// will be sent through an event to the message dispatcher.
        /// </remarks>
        private void ProcessAcknowledgingMessageBodyState()
        {
            Console.WriteLine("Entering ProcessAcknowledgingMessageBodyState");
            Messages.Message acknowledgementMessage;
            if (_lastStatusCode != StatusCodes.CompletedOK)
            {
                acknowledgementMessage = new Messages.Message(PacketType.Nak, Esp32Interfaces.Transport, 0, 0, (UInt32)_lastStatusCode, null);
            }
            else
            {
                Messages.Message inbound = _inboundMessage.Value;
                acknowledgementMessage = new Messages.Message(PacketType.Ack, (Esp32Interfaces) inbound.Interface, inbound.Function, inbound.MessageID, (UInt32) StatusCodes.CompletedOK, null);
            }
            byte[] encodedAcknowledgementMessage = Encoders.EncodeMessage(acknowledgementMessage);
            _esp32.WriteBytes(encodedAcknowledgementMessage);
            _currentState = SpiStates.Idle;
            if (_lastStatusCode != StatusCodes.CompletedOK)
            {
                MessageReceviedEventArgs args = new MessageReceviedEventArgs(_inboundMessage.Value);
                MessageReceived(this, args);
            }
            _inboundMessage = null;
            _lastStatusCode = StatusCodes.CompletedOK;
            _stm32ReadyPin.State = false;
        }

        /// <summary>
        /// ESP32 has indicated that it is ready so send the message header.
        /// </summary>
        private void ProcessWantToSendMessageHeaderState()
        {
            Console.WriteLine("Entering ProcessWantToSendMessageHeaderState");
            byte[] encodedHeader;
            if (_outboundMessage.Value.MessageType == (byte) PacketType.Data)
            {
                Messages.Message outbound = _outboundMessage.Value;
                Messages.Message headerMessage = new Messages.Message(PacketType.Header, (Esp32Interfaces) outbound.Interface, outbound.Function, outbound.MessageID, outbound.StatusCode, null);
                //
                //  For a header we set the payload to null (which the constructor uses
                //  to set the PayloadLength to 0) so we must put the PayloadLength into
                //  the header.
                //
                headerMessage.PayloadLength = outbound.PayloadLength;
                encodedHeader = Encoders.EncodeMessage(headerMessage, true);
            }
            else
            {
                encodedHeader = Encoders.EncodeMessage((Messages.Message) _outboundMessage, true);
            }
            _esp32.WriteBytes(encodedHeader);
            _currentState = SpiStates.WaitingForMessageHeaderAcknowledgement;
            _stm32ReadyPin.State = false;
            _stm32ReadyPin.State = true;
        }

        /// <summary>
        /// Process the ESP32 acknowledgement message.
        /// </summary>
        /// <remarks>
        /// The ESP32 should only really send an ACK or a NAK message through to
        /// the STM32.
        ///
        /// NAK: Generate an error event and throw away the outbound message.
        ///
        /// ACK: Work out if we have a message body to send.
        /// </remarks>
        private void ProcessWaitingForMessageHeaderAcknowledgementState()
        {
            Console.WriteLine("Entering ProcessWaitingForMessageHeaderAcknowledgementState");

            byte[] dummyHeader = new byte[_headerFrameSize];
            Array.Clear(dummyHeader, 0, (int)_headerFrameSize);
            byte[] encodedAcknowledgement = _esp32.WriteRead(dummyHeader, (ushort)_headerFrameSize);
            StatusCodes status;
            Messages.Message? acknowledgement;
            (acknowledgement, status) = Encoders.ExtractMessage(encodedAcknowledgement);
            //
            //  We can get three possible responses here:
            //      ACK - Data received by the ESP32 is good.
            //      NAK - There is a problem with the data, this could be a CRC error or invalid data.
            //
            if (status == StatusCodes.CompletedOK)
            {
                Console.WriteLine("Status OK");
                if (acknowledgement == null)
                {
                    Console.WriteLine("Acknowledgement is null");
                }
                if (acknowledgement.Value.MessageType == (byte)PacketType.Nak)
                {
                    TransportProviderEventArgs args = new TransportProviderEventArgs((Esp32Interfaces)acknowledgement.Value.Interface, acknowledgement.Value.MessageID, (StatusCodes)acknowledgement.Value.StatusCode);
                    TransportError(this, args);
                    _outboundMessage = null;
                    _currentState = SpiStates.Idle;
                }
                else
                {
                    if ((acknowledgement.Value.MessageID == _outboundMessage.Value.MessageID) && (acknowledgement.Value.Interface == _outboundMessage.Value.Interface))
                    {
                        if (_outboundMessage.Value.PayloadLength > 0)
                        {
                            _currentState = SpiStates.WantToSendMessageBody;
                        }
                        else
                        {
                            _outboundMessage = null;
                            _currentState = SpiStates.Idle;
                        }
                    }
                    else
                    {
                        TransportProviderEventArgs args = new TransportProviderEventArgs((Esp32Interfaces)acknowledgement.Value.Interface, acknowledgement.Value.MessageID, StatusCodes.UnexpectedData);
                        TransportError(this, args);
                        _currentState = SpiStates.Idle;
                    }
                }
            }
            else
            {
                TransportProviderEventArgs args = new TransportProviderEventArgs(Esp32Interfaces.Transport, Messages.INVALID_MESSAGE_ID, status);
                TransportError(this, args);
                _currentState = SpiStates.Idle;
            }
            _stm32ReadyPin.State = false;
            if (_currentState == SpiStates.WantToSendMessageBody)
            {
                _stm32ReadyPin.State = true;
            }
        }

        /// <summary>
        /// Process the sending message body state.
        /// </summary>
        private void ProcessWantToSendMessageBodyState()
        {
            Console.WriteLine("Entering ProcessWantToSendMessageBodyState");
            byte[] encodedMessage = Encoders.EncodeMessage((Messages.Message) _outboundMessage, false);
            _esp32.WriteBytes(encodedMessage);
            _currentState = SpiStates.WaitingForMessageBodyAcknowledgement;
            _stm32ReadyPin.State = false;
            _stm32ReadyPin.State = true;
        }

        /// <summary>
        /// Process the incoming message body acknowledgement.
        /// </summary>
        private void ProcessWaitingForMessageBodyAcknowledgementState()
        {
            Console.WriteLine("Entering ProcessWaitingForMessageBodyAcknowledgementState");

            byte[] dummyHeader = new byte[_headerFrameSize];
            Array.Clear(dummyHeader, 0, (int)_headerFrameSize);
            byte[] encodedAcknowledgement = _esp32.WriteRead(dummyHeader, (ushort)_headerFrameSize);
            StatusCodes status;
            Messages.Message? acknowledgement;
            (acknowledgement, status) = Encoders.ExtractMessage(encodedAcknowledgement);
            //
            //  We can get three possible responses here:
            //      ACK - Data received by the ESP32 is good.
            //      NAK - There is a problem with the data, this could be a CRC error or invalid data.
            //
            if (status == StatusCodes.CompletedOK)
            {
                if (acknowledgement.Value.MessageType == (byte)PacketType.Nak)
                {
                    TransportProviderEventArgs args = new TransportProviderEventArgs((Esp32Interfaces)acknowledgement.Value.Interface, acknowledgement.Value.MessageID, (StatusCodes)acknowledgement.Value.StatusCode);
                    TransportError(this, args);
                    _outboundMessage = null;
                    _currentState = SpiStates.Idle;
                }
                else
                {
                    if ((acknowledgement.Value.MessageID == _outboundMessage.Value.MessageID) && (acknowledgement.Value.Interface == _outboundMessage.Value.Interface))
                    {
                        _outboundMessage = null;
                        _currentState = SpiStates.Idle;
                    }
                    else
                    {
                        TransportProviderEventArgs args = new TransportProviderEventArgs((Esp32Interfaces)acknowledgement.Value.Interface, acknowledgement.Value.MessageID, StatusCodes.UnexpectedData);
                        TransportError(this, args);
                    }
                }
            }
            else
            {
                TransportProviderEventArgs args = new TransportProviderEventArgs(Esp32Interfaces.Transport, Messages.INVALID_MESSAGE_ID, status);
                TransportError(this, args);
            }
            _currentState = SpiStates.Idle;
            _stm32ReadyPin.State = false;
        }

        /// <summary>
        /// Process the Transport packet transmission state.
        /// </summary>
        private void ProcessWantToSendTransportMessageState()
        {
        }

        /// <summary>
        /// Queue a for sending to the ESP32.
        /// </summary>
        /// <param name="message">Message to be queued.</param>
        public override void QueueMessageForEsp32(Messages.Message message)
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
