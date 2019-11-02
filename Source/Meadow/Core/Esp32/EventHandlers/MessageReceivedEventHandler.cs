using System;
using System.Collections.Generic;
using System.Text;

namespace Meadow.Core.Esp32
{
    /// <summary>
    /// 
    /// </summary>
    public class MessageReceviedEventArgs
    {
        #region Properties

        /// <summary>
        ///     Message received from the ESP32.
        /// </summary>
        public Messages.Message? Message { get; private set; } = null;

        #endregion Properties

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        private MessageReceviedEventArgs() { }

        /// <summary>
        ///     Create a new MessageReceivedEventArgs object.
        /// </summary>
        /// <param name="message">Message received from the ESP32.</param>
        public MessageReceviedEventArgs(Messages.Message message)
        {
            Message = message;
        }

        #endregion Constructors
    }

    #region Delegates

    /// <summary>
    ///     Delegate for the MessageQueue message received event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">MessageReceviedEventArgs containing the information about the message received.</param>
    public delegate void MessageReceivedEventHandler(object sender, MessageReceviedEventArgs e);

    #endregion Delegates
}
