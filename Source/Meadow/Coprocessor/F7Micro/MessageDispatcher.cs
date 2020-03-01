using System;
using System.Runtime.CompilerServices;
using System.Collections.Generic;
using System.Threading;
using Meadow.Coprocessor.F7Micro.Exceptions;
using Meadow.Hardware;
using Meadow.Coprocessor.F7Micro;

namespace Meadow.Coprocessor.F7Micro
{
    /// <summary>
    /// The MessageDispatcher class is responsible for processing messages for the network
    /// coprocessor.
    ///
    /// Messages from the application are queued ready to be sent to the coprocessor and responses
    /// from the coprocessor are passed back to the calling application.
    /// </summary>
    public class MessageDispatcher
    {
        #region Constants

        /// <summary>
        /// Mask used to differentiate ESP32 messages from STM32 messages.  ESP32 message will have
        /// the top bit set, STM32 messages will have the top bit cleared. 
        /// </summary>
        private const UInt32 ESP32_MESSAGE_ID_MASK = 0x80000000;
        
        #endregion Constants
        
        #region Enums
        
        /// <summary>
        /// Possible debug outputs.
        /// </summary>
        [Flags]
        public enum DebugOptions { None = 0x00, Information = 0x01, Errors = 0x02 }
        
        #endregion
        
        #region Private variables.

        /// <summary>
        /// Serial port connecting the STM32 and the ESP32 microcontrollers.
        /// </summary>
        private readonly TransportProviderBase _esp32;

        /// <summary>
        /// ID of the last message sent to the coprocessor.
        /// </summary>
        private UInt32 _lastMessageId = 0;

        /// <summary>
        /// List of the messages that have been submitted to the coprocessor.
        /// </summary>
        private readonly Dictionary<UInt32, MessageListEntry> _messageList = new Dictionary<uint, MessageListEntry>();

        /// <summary>
        /// Object used to lock the message list to prevent it from having an entry
        /// added to it and removed at the same time.
        /// </summary>
        private readonly object _messageListLock = new Object();
        
        #endregion Private variables

        #region Properties
        
        /// <summary>
        /// Determine how much debug output is generated.
        /// </summary>
        public DebugOptions DebugLevel { get; set; }
        
        #endregion Properties
        
        #region Delegates

        /// <summary>
        /// Error event to allow lower level errors to bubble up to the interfaces.
        /// </summary>
        public event MessageDispatcherErrorEventHandler Error = delegate { };

        #endregion Delegates

        #region Constructors

        /// <summary>
        /// Create a new MessageQueue object and attach it to the specified COM port.
        /// </summary>
        public MessageDispatcher(IIODevice device, IPin mosi, IPin miso, IPin clock, IPin chipSelect, IPin bootPin, IPin resetPin, IPin messageWaitingPin)
        {
            DebugLevel = DebugOptions.None;
            if (_esp32 != null)
            {
                throw new InvalidOperationException("Only a single instance of a MessageDispatcher can be created.");
            }
            _esp32 = new SpiTransportProvider(device, mosi, miso, clock, chipSelect, bootPin, resetPin, messageWaitingPin);
            _esp32.MessageReceived += _esp32_DataReceived;
            _esp32.TransportError += _esp32_TransportError;
        }

        #endregion Constructors

        #region Event handlers

        /// <summary>
        /// Pass on any error information the from the transport layer to the calling application.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e">Event information (error data)</param>
        private void _esp32_TransportError(object sender, TransportProviderErrorEventArgs e)
        {
            MessageDispatcherErrorEventArgs args = new MessageDispatcherErrorEventArgs(e.Interface, e.MessageId, e.StatusCode);
            Error(this, args);
        }

        /// <summary>
        /// Take the message from the coprocessor and pass the result on to the calling application.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e">Data (as a message) received from the coprocessor.</param>
        private void _esp32_DataReceived(object sender, TransportProviderMessageReceivedEventArgs e)
        {
            lock (_messageListLock)
            {
                MessageListEntry entry;
                if (_messageList.TryGetValue(e.MessageReceived.MessageID, out entry))
                {
                    entry.Response = e.MessageReceived;
                    if (entry.WaitHandle != null)
                    {
                        entry.WaitHandle.Set();
                    }
                    else
                    {
                        // TODO: Work out how to deal with async calls.
                    }
                }
                else
                {
                    throw new CoprocessorResponseNotFoundException($"Message list entry for message ID {e.MessageReceived.MessageID} not found.");
                }
            }
        }

        #endregion Event handlers

        #region Methods

        /// <summary>
        /// Send a message to the coprocessor and wait for the response.
        /// </summary>
        /// <param name="message">Message to be sent.</param>
        /// <returns>Response from the coprocessor.</returns>
        /// <exception cref="CoprocessorException">Thrown if the message dispatching system receives an error code from the coprocessor.</exception>
        public Messages.Message SendMessage(Messages.Message message)
        {
            EventWaitHandle waitHandle = new AutoResetEvent(false);
            //
            //  Queue the request.
            //
            Add(message, waitHandle);
            //
            //  Wait for the response to be ready.
            //
            waitHandle.WaitOne();
            //
            //  Now get the response.
            //
            var response = GetResponse(message.MessageID);
            var result = (StatusCodes) response.StatusCode;
            if (result != StatusCodes.CompletedOK)
            {
                throw new CoprocessorException(result);
            }

            return (response);
        }
        
        /// <summary>
        /// Send a message to the coprocessor and wait for the response.
        /// </summary>
        /// <param name="message">Message to be sent.</param>
        /// <returns>Response from the coprocessor.</returns>
        public void SendMessageAsync(Messages.Message message)
        {
            Add(message, null);
        }
        
        /// <summary>
        /// Add a new message to the message queue.
        /// </summary>
        /// <param name="message">Message to add to the queue.</param>
        /// <param name="waitHandle">Wait handle that will be used to awaken the calling thread when a result has been received from the coprocessor.</param>
        [MethodImpl(MethodImplOptions.Synchronized)]
        private void Add(Messages.Message message, EventWaitHandle waitHandle)
        {
            MessageListEntry entry = new MessageListEntry(waitHandle);
            lock (_messageListLock)
            {
                _messageList.Add(message.MessageID, entry);
            }
            _esp32.QueueMessageForEsp32(message);
        }

        /// <summary>
        /// Get the response from the message list and return it to the caller.
        ///
        /// The message list entry will be removed once the result has been retrieved.
        /// </summary>
        /// <param name="messageId">ID of the message the caller wants the response to.</param>
        /// <returns>Message returned by the coprocessor.</returns>
        private Messages.Message GetResponse(UInt32 messageId)
        {
            Messages.Message response;
            lock (_messageListLock)
            {
                MessageListEntry entry;
                if (_messageList.TryGetValue(messageId, out entry))
                {
                    if (entry.Response != null)
                    {
                        response = entry.Response.Value;
                        _messageList.Remove(messageId);
                    }
                    else
                    {
                        throw new CoprocessorResponseNotFoundException($"Still waiting for a response to message with ID {messageId}.");
                    }
                }
                else
                {
                    throw new CoprocessorResponseNotFoundException($"Response for message ID {messageId} not found.");
                }
            }
            return (response);
        }

        /// <summary>
        /// Get the next message ID that can be used in a message to be sent to the
        /// coprocessor.
        ///
        /// This method relies upon the fact that MethodImpl(MethodImplOptions.Synchronized) will
        /// make sure that access to _lastMessageId is thread safe.
        /// </summary>
        /// <returns>Valid message ID to use when sending a message from the STM32 to the ESP32.</returns>
        [MethodImpl(MethodImplOptions.Synchronized)]
        public UInt32 GetNextMessageId()
        {
            _lastMessageId++;
            return (_lastMessageId &~ESP32_MESSAGE_ID_MASK);
        }

        #endregion Methods
    }
}
