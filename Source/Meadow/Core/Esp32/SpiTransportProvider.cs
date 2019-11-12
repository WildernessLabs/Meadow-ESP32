using System;
using System.Threading;
using System.Collections.Generic;
using Meadow.Hardware;
using System.Collections;

namespace Meadow.Core.Esp32
{
    /// <summary>
    /// Provide services to transfer data between the STM32 and the ESP32 using the SPI bus.
    /// </summary>
    public class SpiTransportProvider : TransportProviderBase
    {
        #region Enums
        
        /// <summary>
        /// Possible debug outputs.
        /// </summary>
        [Flags]
        public enum DebugOptions : UInt32 { None = 0x00, Information = 0x01, Errors = 0x02, MethodNames = 0x04, Messages = 0x08, DataPackets = 0x10, Full = 0xffffffff }
        
        #endregion
        
        #region Constants

        /// <summary>
        /// Speed to the SPI bus.
        /// </summary>
        private const ushort SPI_BUS_SPEED = 375;

        #endregion Constants

        #region Class level variables.
        
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
        private readonly DigitalInputPort _esp32ReadyPin;

        /// <summary>
        /// STM32 pin connected to the boot pin of the ESP32.
        /// </summary>
        private readonly DigitalOutputPort _bootPin;

        /// <summary>
        /// Queue of messages to be sent to the ESP32.
        /// </summary>
        private static Queue _messageQueue = new Queue();

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
        /// This message will be used a lot so we set up an object ready for reuse.
        /// </summary>
        private readonly Messages.Message _requestMessage;

        /// <summary>
        /// This value is used a lot so make it available as a class level variable.
        /// </summary>
        private readonly UInt32 _headerFrameSize = Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE);

        #endregion
        
        #region Properties
        
        /// <summary>
        /// Determine how much debug output is generated.
        /// </summary>
        public DebugOptions DebugLevel { get; set; }
        
        #endregion Properties
        
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
        public SpiTransportProvider(IIODevice device, IPin mosi, IPin miso, IPin clock, IPin chipSelect, IPin bootPin, IPin resetPin)
        {
            DebugLevel = DebugOptions.None;
            SpiClockConfiguration spiConfiguration = new SpiClockConfiguration(SPI_BUS_SPEED, SpiClockConfiguration.Mode.Mode3);
            _esp32 = new SpiPeripheral(device.CreateSpiBus(clock, mosi, miso, spiConfiguration), device.CreateDigitalOutputPort(chipSelect, true));

            _bootPin = (DigitalOutputPort) device.CreateDigitalOutputPort(bootPin, initialState: true);

            _reset = (DigitalOutputPort) device.CreateDigitalOutputPort(resetPin, false);

            Thread.Sleep(1);
            _reset.State = true;
            _bootPin.Dispose();

            _queueHandler = new Thread(() =>
            {
                Run();
            });
            _queueHandler.Start();
            
            _requestMessage = new Messages.Message();
            _requestMessage.MessageType = (byte) MessageTypes.Transport;
            _requestMessage.Interface = (byte) Esp32Interfaces.Transport;
            _requestMessage.Function = (UInt32) TransportFunction.SendResponse;

            _esp32ReadyPin = (DigitalInputPort) device.CreateDigitalInputPort(bootPin, InterruptMode.EdgeRising, ResistorMode.Disabled);
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
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information),"ESP32 has generated an interrupt.");
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
        /// Interfaces queue messages inform the ESP32 of an action to be taken or as a
        /// request for information.
        ///
        /// The "Request" message queued by the ESP32 ready interrupt.
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
                        message = (Messages.Message) _messageQueue.Dequeue();
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
        /// Send the message header to the ESP32.
        /// </summary>
        /// <remarks>
        /// Use the message to generate a header and send the header to the ESP32.
        /// </remarks>
        /// <param name="message">Message that the STM32 wants to send.</param>
        private void SendMessageHeader(Messages.Message message)
        {
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.MethodNames), "Entering SendMessageHeader");
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
        /// Get the acknowledgement for the message that has just been sent to the ESP32. 
        /// </summary>
        /// <remarks>
        /// The ESP32 should only really send an ACK or a NAK message through to
        /// the STM32.
        ///
        /// NAK: Generate an error event and throw away the outbound message.
        ///
        /// ACK: Work out if we have a message body to send.
        ///
        /// Transport Message: System message from the ESP32.
        /// </remarks>
        private (Messages.Message?, StatusCodes) GetMessageHeaderAcknowledgement(Messages.Message message)
        {
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.MethodNames), "Entering GetMessageHeaderAcknowledgement");
            byte[] encodedAcknowledgement = _esp32.ReadBytes((ushort) _headerFrameSize); // _esp32.WriteRead(dummyHeader, (ushort)_headerFrameSize);

            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.DataPackets), "Received:");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.DataPackets), DebugHelpers.DebugHelpers.PrintableBuffer(encodedAcknowledgement));
            
            StatusCodes decodeStatus;
            Messages.Message? acknowledgement;
            (acknowledgement, decodeStatus) = Encoders.ExtractMessage(encodedAcknowledgement);

            if (decodeStatus == StatusCodes.CompletedOK)
            {
                acknowledgement.Value.Log(DebugLevel.HasFlag(DebugOptions.Messages));
                switch ((MessageTypes) acknowledgement.Value.MessageType)
                {
                    case MessageTypes.Ack:
                        if ((acknowledgement.Value.Interface != message.Interface) || (acknowledgement.Value.MessageID != message.MessageID))
                        {
                            decodeStatus = StatusCodes.UnexpectedData;
                        }
                        break;
                    case MessageTypes.Transport:
                        //
                        //  TODO: Need to process the transport messages.
                        //
                        break;
                    case MessageTypes.Nak:
                        //
                        //  TODO: Process a NAK.
                        //
                        break;
                }
            }
            else
            {
                DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "Cannot decode message header.");
                acknowledgement = null;
                decodeStatus = StatusCodes.UnexpectedData;
            }

            return (acknowledgement, decodeStatus);
        }

        /// <summary>
        /// Get a message header from the ESP32.
        /// </summary>
        /// <returns>Message header if a valid header is read, null otherwise.</returns>
        private Messages.Message? GetMessageHeader()
        {
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.MethodNames), "Entering GetMessageHeader");
            
            ushort incomingMessageLength = (ushort) Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE);
            byte[] encodedInboundMessage = _esp32.ReadBytes(incomingMessageLength);

            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.DataPackets),"Received:");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.DataPackets), DebugHelpers.DebugHelpers.PrintableBuffer(encodedInboundMessage));
            StatusCodes decodeStatus;
            Messages.Message? inboundMessage;
            (inboundMessage, decodeStatus) = Encoders.ExtractMessage(encodedInboundMessage, headerOnly: true);
            
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information),$"Decode status: {decodeStatus}");
            SendAcknowledgement(inboundMessage.Value, StatusCodes.CompletedOK);

            return (inboundMessage);
        }

        /// <summary>
        /// Get the message body associated with the specified header.
        /// </summary>
        /// <param name="header">Message header that has been sent ot he ESP32.</param>
        /// <returns>Message and body (or null in the case of an error).</returns>
        private Messages.Message? GetMessageBody(Messages.Message header)
        {
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.MethodNames), "Entering GetMessageBody");

            ushort incomingMessageLength = (ushort) Encoders.CalculateSpiBufferSize(Messages.HEADER_SIZE + header.PayloadLength);
            byte[] encodedInboundMessage = _esp32.ReadBytes(incomingMessageLength);

            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.DataPackets),"Received:");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.DataPackets), DebugHelpers.DebugHelpers.PrintableBuffer(encodedInboundMessage));
            
            StatusCodes decodeStatus;
            Messages.Message? inboundMessage;
            (inboundMessage, decodeStatus) = Encoders.ExtractMessage(encodedInboundMessage);
            
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information),$"Decode status: {decodeStatus}");
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
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.MethodNames), "Entering GetResponseFromEsp32");

            StatusCodes statusCode = StatusCodes.Failure;
            Messages.Message? header = null;
            Messages.Message? message = null;

            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "Requesting header.");
            header = GetMessageHeader();
            if (header != null)
            {
                if (header.Value.PayloadLength > 0)
                {
                    DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "Requesting body.");
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
        /// Create an acknowledgement message and send this to the ESP32.
        /// </summary>
        /// <remarks>
        /// The acknowledgement can be either positive (and ACK) or a negative (NAK) acknowledgement.  The
        /// exact type of message will be determined by the status code.
        /// </remarks>
        /// <param name="message">Message that has been received from the ESP32.</param>
        /// <param name="statusCode">Status code to send in the acknowledgement.</param>
        private void SendAcknowledgement(Messages.Message message, StatusCodes statusCode)
        {
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.MethodNames), "Entering SendAcknowledgement");

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
        /// Process the transport message.
        /// </summary>
        /// <param name="transportMessage">Transport message to be processed.</param>
        /// <returns>Status code indicating if the transport message was processed OK.</returns>
        private StatusCodes ProcessTransportMessage(Messages.Message? transportMessage)
        {
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.MethodNames), "Entering ProcessTransportMessage");

            StatusCodes statusCode = StatusCodes.Failure;
            if (transportMessage != null)
            {
                Messages.Message message = transportMessage.Value;
                message.Log(DebugLevel.HasFlag(DebugOptions.Messages));
            
                switch ((TransportFunction) message.Function)
                {
                    case TransportFunction.SendResponse:
                        if ((StatusCodes) message.StatusCode == StatusCodes.CompletedOK)
                        {
                            statusCode = GetResponseFromEsp32();
                            if (statusCode == StatusCodes.NoMessagesWaiting)
                            {
                                statusCode = StatusCodes.CompletedOK;
                            }
                        }
                        else
                        {
                            if ((StatusCodes) message.StatusCode == StatusCodes.NoMessagesWaiting)
                            {
                                statusCode = StatusCodes.CompletedOK;
                            }
                            else
                            {
                                DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Errors), $"Unknown status code: {message.StatusCode}");
                                statusCode = (StatusCodes) message.StatusCode;
                            }
                        }
                        break;
                    default:
                        DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Errors), $"Unknown transport response, function: {message.Function}, status code: {message.StatusCode}");
                        break;
                }
            }
            return (statusCode);
        }

        /// <summary>
        /// Send the message body in the message to the ESP32.
        /// </summary>
        /// <param name="message">Message containing the body to send to the ESP32.</param>
        /// <returns>Status code indicating if the message body was send and acknowledged OK.</returns>
        private StatusCodes SendMessageBody(Messages.Message message)
        {
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel == DebugOptions.MethodNames, "Entering SendMessageBody");
            StatusCodes statusCode = StatusCodes.Failure;
            
            byte[] encodedMessage = Encoders.EncodeMessage(message, false);
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Messages),"Sending message body:");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Messages), DebugHelpers.DebugHelpers.PrintableBuffer(encodedMessage));
            _esp32.WriteBytes(encodedMessage);
            //
            //  TODO: The following pause is necessary to prevent an off by one bit error that can sometime appear in the acknowledgement.
            //
            Thread.Sleep(50);
            
            byte[] encodedAcknowledgement = _esp32.ReadBytes((ushort)_headerFrameSize);
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Messages), "Acknowledgement:");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Messages), DebugHelpers.DebugHelpers.PrintableBuffer(encodedAcknowledgement));
            
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
                    //
                    //  TODO: Work out how to deal with this.
                }
                else
                {
                    DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "Received ACK.");
                    if ((acknowledgement.Value.Interface != message.Interface) || (acknowledgement.Value.MessageID != message.MessageID))
                    {
                        statusCode = StatusCodes.UnexpectedData;
                    }
                }
            }
            else
            {
                DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "Cannot decode message body.");
                statusCode = StatusCodes.UnexpectedData;
            }

            return (statusCode);
        }

        /// <summary>
        /// Send a message to the ESP32.
        /// </summary>
        /// <param name="message">Message to be sent to the ESP32.</param>
        private void SendMessage(Messages.Message message)
        {
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "****************************************************************************************************");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "Entering SendMessage");
            DebugHelpers.DebugHelpers.LogMessage(DebugLevel.HasFlag(DebugOptions.Information), "Sending message:");
            message.Log(DebugLevel.HasFlag(DebugOptions.Information));
            
            StatusCodes statusCode = StatusCodes.Failure;
            Messages.Message? acknowledgement = null;

            SendMessageHeader(message);
            (acknowledgement,statusCode) = GetMessageHeaderAcknowledgement(message);
            if (statusCode != StatusCodes.CompletedOK)
            {
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
                    if (message.PayloadLength > 0)
                    {
                        statusCode = SendMessageBody(message);
                    }
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
