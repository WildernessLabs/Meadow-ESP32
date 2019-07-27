using System;
using System.Threading;
using System.Collections.Generic;
using System.Diagnostics;
using System.Dynamic;
using Meadow.Hardware;

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
        private readonly SpiPeripheral _esp32;

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
//        private StatusCodes _lastStatusCode = StatusCodes.CompletedOK;

        /// <summary>
        /// Current message being sent by the system.
        /// </summary>
        /// <remarks>
        /// This needs to be a class level variable as it is processed by a
        /// state machine which is implemented by a number of different methods.
        /// </remarks>
        private Messages.Message? _outboundMessage = null;

        /// <summary>
        /// Message used to hold the acknowledgements between message transmission states.
        /// </summary>
        
        private Messages.Message _acknowledgementMessage;

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
        public SpiTransportProvider(IIODevice device, IPin esp32ReadyPin, IPin mosi, IPin miso, IPin clock, IPin chipSelect, IPin bootPin, IPin resetPin)
        {
            //
            //    Create the reset pin first and hold the ESP32 in reset while the other pins are
            //    being instanciated.  This will help prevent any "fake" messages being sent to
            //    the ESP32.
            //
            _reset = (DigitalOutputPort) device.CreateDigitalOutputPort(resetPin, false);
            
//            _stm32ReadyPin = (DigitalOutputPort) device.CreateDigitalOutputPort(stm32ReadyPin, false);
            _esp32ReadyPin = (DigitalInputPort) device.CreateDigitalInputPort(esp32ReadyPin, interruptMode: InterruptMode.EdgeRising);
            _esp32ReadyPin.Changed += _esp32ReadyPin_Changed;
            _esp32 = new SpiPeripheral(new SoftwareSPIBus(device, mosi, miso, clock), device.CreateDigitalOutputPort(chipSelect, true));
//            if (stm32ReadyPin == bootPin)
//            {
//                _boot = _stm32ReadyPin;
//            }
//            else
//            {
                _boot = (DigitalOutputPort) device.CreateDigitalOutputPort(bootPin, true);
//            }
//            
//            _boot.State = true;
//            _reset.State = false;
//            _reset.State = true;
////            _boot.Dispose();
//            _boot = null;

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
                WaitForNotification();
                //
                //  Signals are used to transition from one state to another.
                //
//                Console.WriteLine($"Current state: {_currentState}");
//                switch (_currentState)
//                {
//                    case SpiStates.Idle:
//                        ProcessIdleState();
//                        break;
//                    case SpiStates.AcknowledgingMessageHeader:
//                        ProcessAcknowledgingMessageHeaderState();
//                        break;
//                    case SpiStates.WaitingForMessageBody:
//                        ProcessWaitingForMessageBodyState();
//                        break;
//                    case SpiStates.AcknowledgingMessageBody:
//                        ProcessAcknowledgingMessageBodyState();
//                        break;
//                    case SpiStates.WantToSendMessageHeader:
//                        ProcessWantToSendMessageHeaderState();
//                        break;
//                    case SpiStates.WaitingForMessageHeaderAcknowledgement:
//                        ProcessWaitingForMessageHeaderAcknowledgementState();
//                        break;
//                    case SpiStates.WantToSendMessageBody:
//                        ProcessWantToSendMessageBodyState();
//                        break;
//                    case SpiStates.WaitingForMessageBodyAcknowledgement:
//                        ProcessWaitingForMessageBodyAcknowledgementState();
//                        break;
//                    case SpiStates.WantToSendTransportMessage:
//                        ProcessWantToSendTransportMessageState();
//                        break;
//                    default:
//                        break;
//                }
            }
        }

        /// <summary>
        /// Wait for a message notification to come from the message queue or the ESP32.
        /// </summary>
        private void WaitForNotification()
        {
            //
            //  Work out if we should wait for an incoming signal.
            //
            //  If we just wait then the application will process only the first
            //  message in the queue as only one signal will be processed even if
            //  more than one signals are raised.
            //
            bool wait = true;
//            if (_currentState == SpiStates.Idle)
//            {
//                lock (_messageQueueLock)
//                {
//                    wait = (_messageQueue.Count == 0);
//                }
//            }
            if (wait)
            {
                _waitingForStuffToDoHandle.WaitOne();
                lock (_messageQueueLock)
                {
                    if (_messageQueue.Count > 0)
                    {
                        Messages.Message message = _messageQueue.Dequeue();
//                        _outboundMessage = _messageQueue.Dequeue();
//                        _currentState = SpiStates.WantToSendMessageHeader;
                        _currentRetryCount = 0;
                        SendMessage(message);
                    }
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
//                if (_messageQueue.Count > 0)
//                {
//                    _outboundMessage = _messageQueue.Dequeue();
//                    _currentState = SpiStates.WantToSendMessageHeader;
//                    _currentRetryCount = 0;
////                    _stm32ReadyPin.State = true;
//                }
//                else
                {
                    Messages.Message readyMessage = new Messages.Message(MessageTypes.Ready, Esp32Interfaces.None, Messages.INVALID_FUNCTION, Messages.INVALID_MESSAGE_ID, 0, null);
                    byte[] encodedReadyMessage = Encoders.EncodeMessage(readyMessage, true);
//                    _stm32ReadyPin.State = true;
                    byte[] encodedIncomingHeader = _esp32.WriteRead(encodedReadyMessage, (ushort) encodedReadyMessage.Length);
                    StatusCodes decodeStatus;
                    (_inboundMessage, decodeStatus) = Encoders.ExtractMessage(encodedIncomingHeader);
                    if (decodeStatus == StatusCodes.CompletedOK)
                    {
                        _acknowledgementMessage = new Messages.Message(_inboundMessage.Value);
                        _acknowledgementMessage.MessageType = (byte) MessageTypes.Ack;
                    }
                    else
                    {
                        _acknowledgementMessage = new Messages.Message(MessageTypes.Nak, Esp32Interfaces.Transport, 0, 0, (UInt32) decodeStatus, null);
                        _acknowledgementMessage.MessageType = (byte)MessageTypes.Nak;
                    }
                    _acknowledgementMessage.StatusCode = (UInt32) decodeStatus;
                    _currentState = SpiStates.AcknowledgingMessageHeader;
//                    _stm32ReadyPin.State = false;
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
            byte[] encodedAcknowledgementMessage = Encoders.EncodeMessage(_acknowledgementMessage);
            Console.WriteLine("Sending acknowledgement:");
            Console.WriteLine(DebugHelpers.DebugHelpers.PrintableBuffer(encodedAcknowledgementMessage));
            _esp32.WriteBytes(encodedAcknowledgementMessage);
            if ((MessageTypes) _acknowledgementMessage.MessageType == MessageTypes.Nak)
            {
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
                    _inboundMessage = null;
                    _currentState = SpiStates.Idle;
                }
            }
            _stm32ReadyPin.State = false;
            if (_currentState == SpiStates.WaitingForMessageBody)
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
            
            ushort incomingMessageLength = (ushort) Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE + _inboundMessage.Value.PayloadLength);
            Console.WriteLine($"Asking for {incomingMessageLength} bytes.");
            byte[] encodedDummyMessage = new byte[incomingMessageLength];
            Array.Clear(encodedDummyMessage, 0, incomingMessageLength);
            byte[] encodedInboundMessage = _esp32.WriteRead(encodedDummyMessage, incomingMessageLength);
            Console.WriteLine("Received:");
            Console.WriteLine(DebugHelpers.DebugHelpers.PrintableBuffer(encodedInboundMessage));
            
            StatusCodes decodeStatus;
            Messages.Message? inboundMessage;
            (inboundMessage, decodeStatus) = Encoders.ExtractMessage(encodedInboundMessage);
            Console.WriteLine($"Decode status: {decodeStatus}");
            if (decodeStatus != StatusCodes.CompletedOK)
            {
                //
                //    TODO: Add retry logic.
                //
                _acknowledgementMessage.StatusCode = (UInt32) decodeStatus;
            }
            else
            {
                Console.WriteLine("Decoded message:");
                Console.WriteLine(_inboundMessage.ToString());
                if ((inboundMessage.Value.Interface == _inboundMessage.Value.Interface) && 
                    (inboundMessage.Value.MessageID == _inboundMessage.Value.MessageID) &&
                    (inboundMessage.Value.Function == _inboundMessage.Value.Function))
                {
                    _inboundMessage = inboundMessage;
                    _acknowledgementMessage.StatusCode = (UInt32) StatusCodes.CompletedOK;
                    _acknowledgementMessage.MessageType = (byte) MessageTypes.Ack;
                }
                else
                {
                    //
                    //    Add retry logic.
                    //
                    _acknowledgementMessage.StatusCode = (UInt32) StatusCodes.HeaderBodyFieldMismatch;
                    _acknowledgementMessage.MessageType = (byte) MessageTypes.Ack;
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
            byte[] encodedAcknowledgementMessage = Encoders.EncodeMessage(_acknowledgementMessage);
            _esp32.WriteBytes(encodedAcknowledgementMessage);
            _currentState = SpiStates.Idle;
            if ((StatusCodes) _acknowledgementMessage.StatusCode != StatusCodes.CompletedOK)
            {
                MessageReceviedEventArgs args = new MessageReceviedEventArgs(_inboundMessage.Value);
                MessageReceived(this, args);
            }
            _inboundMessage = null;
            _stm32ReadyPin.State = false;
        }

        /// <summary>
        /// ESP32 has indicated that it is ready so send the message header.
        /// </summary>
        private void ProcessWantToSendMessageHeaderState(Messages.Message message)
        {
            Console.WriteLine("Entering ProcessWantToSendMessageHeaderState");
            byte[] encodedHeader;
            if (message.MessageType == (byte) MessageTypes.Data)
            {
                Messages.Message headerMessage = new Messages.Message(MessageTypes.Header, (Esp32Interfaces) message.Interface, message.Function, message.MessageID, message.StatusCode, null);
                //
                //  For a header we set the payload to null (which the constructor uses
                //  to set the PayloadLength to 0) so we must put the PayloadLength into
                //  the header.
                //
                headerMessage.PayloadLength = message.PayloadLength;
                encodedHeader = Encoders.EncodeMessage(headerMessage, true);
            }
            else
            {
                encodedHeader = Encoders.EncodeMessage((Messages.Message) message, true);
            }
            _esp32.WriteBytes(encodedHeader);
//            _currentState = SpiStates.WaitingForMessageHeaderAcknowledgement;
//            _stm32ReadyPin.State = false;
//            _stm32ReadyPin.State = true;
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
        private StatusCodes ProcessWaitingForMessageHeaderAcknowledgementState(Messages.Message message)
        {
            Console.WriteLine("Entering ProcessWaitingForMessageHeaderAcknowledgementState");

            byte[] dummyHeader = new byte[_headerFrameSize];
            Array.Clear(dummyHeader, 0, (int)_headerFrameSize);
            byte[] encodedAcknowledgement = _esp32.WriteRead(dummyHeader, (ushort)_headerFrameSize);
            Console.WriteLine("Received:");
            Console.WriteLine(DebugHelpers.DebugHelpers.PrintableBuffer(encodedAcknowledgement));
            StatusCodes decodeStatus;
            Messages.Message? acknowledgement;
            (acknowledgement, decodeStatus) = Encoders.ExtractMessage(encodedAcknowledgement);
            //
            //  We can get two possible responses here:
            //      ACK - Data received by the ESP32 is good.
            //      NAK - There is a problem with the data, this could be a CRC error or invalid data.
            //
            if (decodeStatus == StatusCodes.CompletedOK)
            {
                Console.WriteLine("Decoded OK.");
                Console.WriteLine(acknowledgement.ToString());
                if (acknowledgement.Value.MessageType == (byte) MessageTypes.Nak)
                {
                    Console.WriteLine("Received NAK.");
                }
                else
                {
                    Console.WriteLine("Received ACK.");
                    if ((acknowledgement.Value.Interface != message.Interface) || (acknowledgement.Value.MessageID != message.MessageID))
                    {
                        decodeStatus = StatusCodes.UnexpectedData;
                    }
                }
            }
            else
            {
                Console.WriteLine("Cannot decode message header.");
                decodeStatus = StatusCodes.UnexpectedData;
            }

            return (decodeStatus);
        }

        /// <summary>
        /// Process the sending message body state.
        /// </summary>
        private void ProcessWantToSendMessageBodyState(Messages.Message message)
        {
            Console.WriteLine("Entering ProcessWantToSendMessageBodyState");
            byte[] encodedMessage = Encoders.EncodeMessage((Messages.Message) message, false);
            _esp32.WriteBytes(encodedMessage);
            _currentState = SpiStates.WaitingForMessageBodyAcknowledgement;
//            _stm32ReadyPin.State = false;
//            _stm32ReadyPin.State = true;
        }

        /// <summary>
        /// Process the incoming message body acknowledgement.
        /// </summary>
        private StatusCodes ProcessWaitingForMessageBodyAcknowledgementState(Messages.Message message)
        {
            Console.WriteLine("Entering ProcessWaitingForMessageBodyAcknowledgementState");

            byte[] dummyHeader = new byte[_headerFrameSize];
            Array.Clear(dummyHeader, 0, (int)_headerFrameSize);
            byte[] encodedAcknowledgement = _esp32.WriteRead(dummyHeader, (ushort)_headerFrameSize);
            StatusCodes decodeStatus;
            Messages.Message? acknowledgement;
            (acknowledgement, decodeStatus) = Encoders.ExtractMessage(encodedAcknowledgement);
            //
            //  We can get three possible responses here:
            //      ACK - Data received by the ESP32 is good.
            //      NAK - There is a problem with the data, this could be a CRC error or invalid data.
            //
            if (decodeStatus == StatusCodes.CompletedOK)
            {
                if (acknowledgement.Value.MessageType == (byte) MessageTypes.Nak)
                {
                    Console.WriteLine("Received NAK.");
                }
                else
                {
                    Console.WriteLine("Received ACK.");
                    if ((acknowledgement.Value.Interface != message.Interface) || (acknowledgement.Value.MessageID != message.MessageID))
                    {
                        decodeStatus = StatusCodes.UnexpectedData;
                    }
                }
            }
            else
            {
                Console.WriteLine("Cannot decode message body.");
                decodeStatus = StatusCodes.UnexpectedData;
            }

            return (decodeStatus);
        }

        private int CheckForRetry(Messages.Message message, StatusCodes statusCode, int retryCount)
        {
            if (statusCode  != StatusCodes.CompletedOK)
            {
                if (retryCount < MaximumRetryCount)
                {
                    retryCount++;
                }
                else
                {
                    RaiseError(message, statusCode);
                }
            }
            else
            {
                retryCount = 0;
            }

            return (retryCount);
        }

        private void SendMessage(Messages.Message message)
        {
            int retryCount = 0;
            StatusCodes statusCode = StatusCodes.Failure;

            while ((retryCount < MaximumRetryCount) && (statusCode != StatusCodes.CompletedOK))
            {
                ProcessWantToSendMessageHeaderState(message);
                statusCode = ProcessWaitingForMessageHeaderAcknowledgementState(message);
//                retryCount = CheckForRetry(message, statusCode, retryCount);
            }
            //
            //    RetryCount will be equal to MaximumRetryCount here if we failed to send
            //    the message header.  In this case we should not send the message body as
            //    the ESP32 application will not have a large enough buffer allocated for
            //    the body if the header was not processed correctly.
            //
            if (retryCount < MaximumRetryCount)
            {
                if (message.PayloadLength > 0)
                {
                    retryCount = 0;
                    statusCode = StatusCodes.Failure;
                    while ((retryCount < MaximumRetryCount) && (statusCode != StatusCodes.CompletedOK))
                    {
                        ProcessWantToSendMessageBodyState(message);
                        statusCode = ProcessWaitingForMessageBodyAcknowledgementState(message);
                        retryCount = CheckForRetry(message, statusCode, retryCount);
                    }
                }
            }
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

        /// <summary>
        /// An error has been detected so check the retry count to see if we can retry sending the message.
        /// </summary>
        /// <remarks>
        /// The system will go back to retrying to send the message header if an error has been detected.
        /// This simplifies the restart process and guarantees that we have a valid header / message body
        /// pair.
        /// </remarks>
        /// <param name="statusCode">Status code that should be used if an error message is generated.</param>
        private void RetrySendingIfPossible(StatusCodes statusCode)
        {
            if (_currentRetryCount < MaximumRetryCount)
            {
                _currentRetryCount++;
                _currentState = SpiStates.WantToSendMessageHeader;
                Console.WriteLine($"    Retrying, count = {_currentRetryCount}");
            }
            else
            {
                TransportProviderEventArgs args = new TransportProviderEventArgs((Esp32Interfaces) _outboundMessage.Value.Interface, _outboundMessage.Value.MessageID, statusCode);
                TransportError(this, args);
                _outboundMessage = null;
                _currentState = SpiStates.Idle;
                _currentRetryCount = 0;
            }
        }

        private void RaiseError(Messages.Message message, StatusCodes statusCode)
        {
            TransportProviderEventArgs args = new TransportProviderEventArgs((Esp32Interfaces) message.Interface, message.MessageID, statusCode);
            TransportError(this, args);
        }

        #endregion
    }
}
