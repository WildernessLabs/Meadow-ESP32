using System;
using System.Collections.Generic;
using System.Text;

namespace Esp32Messaging
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
        public Message Esp32Message { get; private set; } = null;

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
        public MessageReceviedEventArgs(Message message)
        {
            Esp32Message = message;
        }

        #endregion Constructors
    }

    #region Delegates

    /// <summary>
    ///     Delegate for the MessageQueue message received event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">MessageReceviedEventArgs containing the information about the message received.</param>
    public delegate void MessageReceviedEventHandler(object sender, MessageReceviedEventArgs e);

    #endregion Delegates
}
