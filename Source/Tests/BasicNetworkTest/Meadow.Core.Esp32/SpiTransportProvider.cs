using System;
using System.Threading;
using System.Collections.Generic;
using Meadow.Hardware;

namespace Meadow.Core.Esp32
{
    /// <summary>
    /// Provide services to transfer data between the STM32 and the ESP32 using the SPI bus.
    /// </summary>
    public class SpiTransportProvider : TransportProviderBase
    {
        #region Class level variables.

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
        /// Queue of messages to be sent to the ESP32.
        /// </summary>
        private static Queue<Messages.Message> _messageQueue = null;

        /// <summary>
        /// Lock for the message queue to synchronise access by the two threads (main application
        /// adding messages and the queue processing thread taking messages from the queue).
        /// </summary>
        private readonly object _messageQueueLock = new object();

        /// <summary>
        /// Handle used to synchronise the incoming messages and the task processing the messages.
        /// </summary>
        private readonly EventWaitHandle _waitingForStuffToDoHandle = new AutoResetEvent(false);

        /// <summary>
        /// Thread processing the messages as they enter the queue.
        /// </summary>
        private Thread _queueHandler = null;

        /// <summary>
        /// 
        /// </summary>
        private Messages.Message? _inboundMessage = null;

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
        /// This message will be used a lot so we set up an object ready for reuse.
        /// </summary>
        private readonly Messages.Message _requestMessage;

        /// <summary>
        /// This value is used a lot so make it available as a class level variable.
        /// </summary>
        private readonly UInt32 _headerFrameSize = Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE);

        #endregion
        
        #region Constructor(s)

        /// <summary>
        /// Default constructor is private to prevent it from being called.
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
            //    being instantiated.  This will help prevent any "fake" messages being sent to
            //    the ESP32.
            //
            _reset = (DigitalOutputPort) device.CreateDigitalOutputPort(resetPin, false);
            
            _esp32 = new SpiPeripheral(new SoftwareSPIBus(device, mosi, miso, clock), device.CreateDigitalOutputPort(chipSelect, true));
            _boot = (DigitalOutputPort) device.CreateDigitalOutputPort(bootPin, true);

            _reset.State = true;
            _esp32ReadyPin = (DigitalInputPort) device.CreateDigitalInputPort(esp32ReadyPin, interruptMode: InterruptMode.EdgeRising);
            
            _messageQueue = new Queue<Messages.Message>();
            _queueHandler = new Thread(() =>
            {
                Run();
            });
            _queueHandler.Start();
            
            _requestMessage = new Messages.Message();
            _requestMessage.MessageType = (byte)MessageTypes.Transport;
            _requestMessage.Interface = (byte)Esp32Interfaces.Transport;
            _requestMessage.Function = (UInt32)TransportFunction.SendResponse;

            //
            //  We need to wait for 100ms after resetting the ESP32 before attaching
            //  an interrupt as the ESP32 will be sending diagnostic information to
            //  the UART pin.
            //
            Thread.Sleep(100);
            _esp32ReadyPin.Changed += _esp32ReadyPin_Changed;
        }

        #endregion

        #region Delegates and events

        /// <summary>
        /// Error event to allow lower level errors to bubble up to the interfaces.
        /// </summary>
        public override event TransportProviderErrorEventHandler TransportError = delegate { };

        /// <summary>
        /// Message received event.
        /// </summary>
        public override event TransportProviderMessageReceivedEventHandler MessageReceived = delegate { };

        /// <summary>
        /// The ESP32 is indicating that it is ready to process some data from the STM32
        /// or that it has data ready to send to the STM32.
        /// </summary>
        /// <remarks>
        /// Messages are retrieved from the ESP32 by sending a Transport message asking for
        /// the next message from the queue.  The processes becomes a simple case of putting
        /// the request message on the queue and then treating it like any other message to
        /// be sent to the ESP32.
        /// </remarks>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _esp32ReadyPin_Changed(object sender, DigitalInputPortEventArgs e)
        {
            DebugHelpers.DebugHelpers.LogMessage("ESP32 has generated an interrupt.");
            QueueMessageForEsp32(_requestMessage);
        }

        #endregion

        #region Methods

        /// <summary>
        /// Main MessageDispatcher thread.
        /// </summary>
        /// <remarks>
        /// The main thread waits for messages to be added to the queue of outbound messages.
        ///
        /// Messages can either be added by the various interfaces or by the ESP32 "Ready" event.
        ///
        /// Interfaces queue messages to inform the ESP32 of an action to be taken or as a
        /// request for information.
        ///
        /// The "Request" message queued by the ESP32 Ready event asks the ESP32 for any waiting
        /// messages (responses).
        /// </remarks>
        private void Run()
        {
            while (true)
            {
                _waitingForStuffToDoHandle.WaitOne();
                int queueLength = 0;
                lock (_messageQueueLock)
                {
                    queueLength = _messageQueue.Count;
                }
                    
                while (queueLength > 0)
                {
                    Messages.Message message;
                    lock (_messageQueueLock)
                    {
                        message = _messageQueue.Dequeue();
                    }
                    SendMessage(message);
                    lock (_messageQueueLock)
                    {
                        queueLength = _messageQueue.Count;
                    }
                }
            }
        }

        /// <summary>
        /// ESP32 has indicated that it is ready so send the message header.
        /// </summary>
        private void ProcessWantToSendMessageHeaderState(Messages.Message message)
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering ProcessWantToSendMessageHeaderState");
            byte[] encodedHeader;
            if (message.MessageType == (byte) MessageTypes.Data)
            {
                Messages.Message headerMessage = new Messages.Message(MessageTypes.Header, (Esp32Interfaces) message.Interface, message.Function, message.MessageID, message.StatusCode, null);
                //
                //  For a header we set the payload to null (which the constructor uses to set the PayloadLength to 0)
                //  so we must put the PayloadLength into the header.
                //
                headerMessage.PayloadLength = message.PayloadLength;
                encodedHeader = Encoders.EncodeMessage(headerMessage, true);
            }
            else
            {
                encodedHeader = Encoders.EncodeMessage((Messages.Message) message, true);
            }
            _esp32.WriteBytes(encodedHeader);
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
        private (Messages.Message?, StatusCodes) ProcessWaitingForMessageHeaderAcknowledgementState(Messages.Message message)
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering ProcessWaitingForMessageHeaderAcknowledgementState");
            byte[] dummyHeader = new byte[_headerFrameSize];
            Array.Clear(dummyHeader, 0, (int)_headerFrameSize);
            byte[] encodedAcknowledgement = _esp32.WriteRead(dummyHeader, (ushort)_headerFrameSize);

            DebugHelpers.DebugHelpers.LogMessage("Received:");
            DebugHelpers.DebugHelpers.LogMessage(DebugHelpers.DebugHelpers.PrintableBuffer(encodedAcknowledgement));
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
                DebugHelpers.DebugHelpers.LogMessage("Decoded OK.");
                acknowledgement.Value.Log();
                switch ((MessageTypes) acknowledgement.Value.MessageType)
                {
                    case MessageTypes.Ack:
                        DebugHelpers.DebugHelpers.LogMessage("Received ACK.");
                        if ((acknowledgement.Value.Interface != message.Interface) || (acknowledgement.Value.MessageID != message.MessageID))
                        {
                            DebugHelpers.DebugHelpers.LogMessage("Message IDs do not match.");
                            decodeStatus = StatusCodes.UnexpectedData;
                        }
                        break;
                    case MessageTypes.Transport:
                        DebugHelpers.DebugHelpers.LogMessage("Transport packet received.");
                        break;
                    case MessageTypes.Nak:
                        DebugHelpers.DebugHelpers.LogMessage("Received NAK.");
                        break;
                }
            }
            else
            {
                DebugHelpers.DebugHelpers.LogMessage("Cannot decode message header.");
                acknowledgement = null;
                decodeStatus = StatusCodes.UnexpectedData;
            }

            return (acknowledgement, decodeStatus);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="message"></param>
        /// <returns></returns>
        private StatusCodes SendMessageBody(Messages.Message message)
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering SendMessageBody");
            StatusCodes statusCode = StatusCodes.Failure;
            
            if (message.PayloadLength > 0)
            {
                byte[] encodedMessage = Encoders.EncodeMessage(message, false);
                DebugHelpers.DebugHelpers.LogMessage("Sending message body:");
                DebugHelpers.DebugHelpers.LogMessage(DebugHelpers.DebugHelpers.PrintableBuffer(encodedMessage));
                _esp32.WriteBytes(encodedMessage);
                
                byte[] dummyHeader = new byte[_headerFrameSize];
                Array.Clear(dummyHeader, 0, (int)_headerFrameSize);
                byte[] encodedAcknowledgement = _esp32.WriteRead(dummyHeader, (ushort)_headerFrameSize);
                Messages.Message? acknowledgement;
                (acknowledgement, statusCode) = Encoders.ExtractMessage(encodedAcknowledgement);
                //
                //  We can get three possible responses here:
                //      ACK - Data received by the ESP32 is good.
                //      NAK - There is a problem with the data, this could be a CRC error or invalid data.
                //
                if (statusCode == StatusCodes.CompletedOK)
                {
                    if (acknowledgement.Value.MessageType == (byte) MessageTypes.Nak)
                    {
                        DebugHelpers.DebugHelpers.LogMessage("Received NAK.");
                    }
                    else
                    {
                        DebugHelpers.DebugHelpers.LogMessage("Received ACK.");
                        if ((acknowledgement.Value.Interface != message.Interface) || (acknowledgement.Value.MessageID != message.MessageID))
                        {
                            statusCode = StatusCodes.UnexpectedData;
                        }
                    }
                }
                else
                {
                    DebugHelpers.DebugHelpers.LogMessage("Cannot decode message body.");
                    statusCode = StatusCodes.UnexpectedData;
                }
            }
            else
            {
                DebugHelpers.DebugHelpers.LogMessage("No message payload to send.");
                statusCode = StatusCodes.CompletedOK;
            }

            return (statusCode);
        }

        /// <summary>
        /// Get a message header from the ESP32.
        /// </summary>
        /// <returns>Message header if a valid header is read, null otherwise.</returns>
        private Messages.Message? GetMessageHeader()
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering GetMessageHeader");
            
            ushort incomingMessageLength = (ushort) Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE);
            DebugHelpers.DebugHelpers.LogMessage($"Asking for {incomingMessageLength} bytes.");
            
            byte[] encodedDummyMessage = new byte[incomingMessageLength];
            Array.Clear(encodedDummyMessage, 0, incomingMessageLength);
            byte[] encodedInboundMessage = _esp32.WriteRead(encodedDummyMessage, incomingMessageLength);

            DebugHelpers.DebugHelpers.LogMessage("Received:");
            DebugHelpers.DebugHelpers.LogMessage(DebugHelpers.DebugHelpers.PrintableBuffer(encodedInboundMessage));
            StatusCodes decodeStatus;
            Messages.Message? inboundMessage;
            (inboundMessage, decodeStatus) = Encoders.ExtractMessage(encodedInboundMessage);
            DebugHelpers.DebugHelpers.LogMessage($"Decode status: {decodeStatus}");
            SendAcknowledgement(inboundMessage.Value, StatusCodes.CompletedOK);

            return (inboundMessage); 
        }

        private Messages.Message? GetMessageBody(Messages.Message header)
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering GetMessageHeader");

            ushort incomingMessageLength = (ushort) Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE + header.PayloadLength);
            DebugHelpers.DebugHelpers.LogMessage($"Asking for {incomingMessageLength} bytes.");
            
            byte[] encodedDummyMessage = new byte[incomingMessageLength];
            Array.Clear(encodedDummyMessage, 0, incomingMessageLength);
            byte[] encodedInboundMessage = _esp32.WriteRead(encodedDummyMessage, incomingMessageLength);

            DebugHelpers.DebugHelpers.LogMessage("Received:");
            DebugHelpers.DebugHelpers.LogMessage(DebugHelpers.DebugHelpers.PrintableBuffer(encodedInboundMessage));
            StatusCodes decodeStatus;
            Messages.Message? inboundMessage;
            (inboundMessage, decodeStatus) = Encoders.ExtractMessage(encodedInboundMessage);
            DebugHelpers.DebugHelpers.LogMessage($"Decode status: {decodeStatus}");
            SendAcknowledgement(inboundMessage.Value, StatusCodes.CompletedOK);

            return (inboundMessage);
        }

        /// <summary>
        /// The ESP32 has indicated that there is a response / message for the STM32.
        /// Get the message from the ESP32 and process it accordingly.
        /// </summary>
        /// <returns>Status code indicating if this request succeeded or failed.</returns>
        private StatusCodes GetResponseFromEsp32()
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering GetResponseFromEsp32");

            StatusCodes statusCode = StatusCodes.Failure;
            Messages.Message? header = null;
            Messages.Message? message = null;

            DebugHelpers.DebugHelpers.LogMessage("Requesting header.");
            header = GetMessageHeader();
            if (header != null)
            {
                if (header.Value.PayloadLength > 0)
                {
                    DebugHelpers.DebugHelpers.LogMessage("Requesting body.");
                    message = GetMessageBody(header.Value);
                }
                else
                {
                    message = header;
                }

                if (message != null)
                {
                    var args = new TransportProviderMessageReceivedEventArgs(message.Value);
                    MessageReceived(this, args);
                    statusCode = StatusCodes.CompletedOK;
                }
                else
                {
                    statusCode = StatusCodes.Failure;
                }
                statusCode = StatusCodes.CompletedOK;
            }
            
            return (statusCode);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="message"></param>
        /// <param name="statusCode"></param>
        private void SendAcknowledgement(Messages.Message message, StatusCodes statusCode)
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering SendAcknowledgement");

            MessageTypes messageType = MessageTypes.Ack;
            if (statusCode != StatusCodes.CompletedOK)
            {
                messageType = MessageTypes.Nak;
            }

            Messages.Message acknowledgement = new Messages.Message(messageType, (Esp32Interfaces) message.Interface, message.Function, message.MessageID, (UInt32) statusCode, null);
            byte[] encodedMessage = Encoders.EncodeMessage(acknowledgement, false);
            _esp32.WriteBytes(encodedMessage);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="message"></param>
        /// <returns></returns>
        private StatusCodes ProcessTransportMessage(Messages.Message? acknowledgement)
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering ProcessTransportMessage");

            StatusCodes statusCode = StatusCodes.Failure;
            Messages.Message message;
            if (acknowledgement != null)
            {
                message = acknowledgement.Value;
                message.Log();
            
                switch ((TransportFunction) message.Function)
                {
                    case TransportFunction.SendResponse:
                        DebugHelpers.DebugHelpers.LogMessage("Processing SendResponse message");
                        if ((StatusCodes) message.StatusCode == StatusCodes.CompletedOK)
                        {
                            DebugHelpers.DebugHelpers.LogMessage("There are messages waiting, request one.");
                            statusCode = GetResponseFromEsp32();
                            if (statusCode == StatusCodes.NoMessagesWaiting)
                            {
                                DebugHelpers.DebugHelpers.LogMessage("No messages waiting.");
                                statusCode = StatusCodes.CompletedOK;
                            }
                        }
                        else
                        {
                            if ((StatusCodes) message.StatusCode == StatusCodes.NoMessagesWaiting)
                            {
                                DebugHelpers.DebugHelpers.LogMessage("Converting response to CompletedOK");
                                statusCode = StatusCodes.CompletedOK;
                            }
                            else
                            {
                                DebugHelpers.DebugHelpers.LogMessage($"Unknown status code: {message.StatusCode}");
                                statusCode = (StatusCodes) message.StatusCode;
                            }
                        }
                        break;
                    default:
                        DebugHelpers.DebugHelpers.LogMessage($"Unknown transport response, function: {message.Function}, status code: {message.StatusCode}");
                        break;
                }
            }
            return (statusCode);
        }

        /// <summary>
        /// Send a message to the ESP32.
        /// </summary>
        /// <remarks>
        /// 
        /// </remarks>
        /// <param name="message">Message to be sent to the ESP32.</param>
        private void SendMessage(Messages.Message message)
        {
            DebugHelpers.DebugHelpers.LogMessage("Entering SendMessage");
            DebugHelpers.DebugHelpers.LogMessage("Sending message:");
            message.Log();
            
            StatusCodes statusCode = StatusCodes.Failure;
            Messages.Message? acknowledgement = null;

            ProcessWantToSendMessageHeaderState(message);
            (acknowledgement,statusCode) = ProcessWaitingForMessageHeaderAcknowledgementState(message);
            if (statusCode != StatusCodes.CompletedOK)
            {
                DebugHelpers.DebugHelpers.LogMessage("Failed to send message, resubmitting message");
                QueueMessageForEsp32(message);
            }
            else
            {
                if ((MessageTypes) message.MessageType == MessageTypes.Transport)
                {
                    statusCode = ProcessTransportMessage(acknowledgement);
                }
                else
                {
                    statusCode = SendMessageBody(message);
                }
                if (statusCode != StatusCodes.CompletedOK)
                {
                    QueueMessageForEsp32(message);
                }
            }
        }

        /// <summary>
        /// Queue a message for sending to the ESP32.
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
