using System;

namespace Meadow.Core.Esp32
{
    public class MessageDispatcherErrorEventArgs
    {
        #region Properties

        /// <summary>
        ///     Status / error code being bubbled up from the ESP32.
        /// </summary>
        public StatusCodes StatusCode { get; private set; }

        #endregion Properties

        #region Constructors

        /// <summary>
        ///     Create a new message queue error even argument.
        /// </summary>
        /// <param name="statusCode">Error code / infomation.</param>
        public MessageDispatcherErrorEventArgs(StatusCodes statusCode)
        {
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
