using System;

namespace Meadow.Core.Esp32
{
    public class MessageDispatcherErrorEventArgs
    {
        #region Properties

        /// <summary>
        /// ESP32 Interface that generated the error.
        /// </summary>
        public Esp32Interfaces Interface;

        /// <summary>
        /// ID of the message that generated the error.
        /// </summary>
        public UInt32 MessageId;

        /// <summary>
        ///     Status / error code being bubbled up from the ESP32.
        /// </summary>
        public StatusCodes StatusCode { get; private set; }

        #endregion Properties

        #region Constructors

        /// <summary>
        /// Default constructor is private to prevent it from being used.
        /// </summary>
        private MessageDispatcherErrorEventArgs()
        {
        }

        /// <summary>
        ///     Create a new message queue error even argument.
        /// </summary>
        /// <param name="esp32Interface">Interface on the ESP32 that caused the problem.</param>
        /// <param name="messageId">ID of the message that caused the error.</param>
        /// <param name="statusCode">Error code / infomation.</param>
        public MessageDispatcherErrorEventArgs(Esp32Interfaces esp32Interface, UInt32 messageId, StatusCodes statusCode)
        {
            Interface = esp32Interface;
            MessageId = messageId;
            StatusCode = statusCode;
        }

        #endregion Constructors
    }

    #region Delegates

    /// <summary>
    ///     Delegate for the MessageQueue error event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">MessageQueueErrorEventArgs containing the information about the message being processed when the error condition was raised.</param>
    public delegate void MessageDispatcherErrorEventHandler(object sender, MessageDispatcherErrorEventArgs e);

    #endregion Delegates
}
