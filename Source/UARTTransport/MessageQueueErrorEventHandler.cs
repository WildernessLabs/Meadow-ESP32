using System;
using System.Collections.Generic;
using System.Text;

namespace Esp32Messaging
{
    public class MessageQueueErrorEventArgs
    {
        #region Properties

        /// <summary>
        ///     Device (Interface) generating the error.
        /// </summary>
        public byte Device { get; private set; }

        /// <summary>
        ///     Function being requested.
        /// </summary>
        public UInt32 Function { get; private set; }

        /// <summary>
        ///     ID of the message that generated the error.
        /// </summary>
        public UInt32 MessageID { get; private set; }

        /// <summary>
        ///     Status / error code being bubbled up from the ESP32.
        /// </summary>
        public Esp32Messaging.StatusCodes StatusCode { get; private set; }

        #endregion Properties

        #region Constructors

        /// <summary>
        ///     Create a new message queue error even argument.
        /// </summary>
        /// <param name="device">ESP32 Interface / device that has generated the error.</param>
        /// <param name="function">Function being requested.</param>
        /// <param name="messageID">ID of the message that was being processed at the time the error was generated.</param>
        /// <param name="statusCode">Error code / infomation.</param>
        public MessageQueueErrorEventArgs(byte device, UInt32 function, UInt32 messageID, Esp32Messaging.StatusCodes statusCode)
        {
            Device = device;
            Function = function;
            MessageID = messageID;
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
    public delegate void MessageQueueErrorEventHandler(object sender, MessageQueueErrorEventArgs e);

    #endregion Delegates
}
