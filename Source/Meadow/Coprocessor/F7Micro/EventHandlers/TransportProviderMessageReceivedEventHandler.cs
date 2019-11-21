using System;

namespace Meadow.Coprocessor.F7Micro
{
    /// <summary>
    /// 
    /// </summary>
    public class TransportProviderMessageReceivedEventArgs
    {
        #region Properties

        /// <summary>
        /// MessageReceived received from the ESP32
        /// </summary>
        public Messages.Message MessageReceived;
        
        #endregion Properties

        #region Constructors

        /// <summary>
        /// Default constructor is private to prevent it being used.
        /// </summary>
        private TransportProviderMessageReceivedEventArgs()
        {
        }

        /// <summary>
        /// Create a new object TransportProviderMessageReceivedEventArgs containing
        /// information about the message that has been received.
        /// </summary>
        /// <param name="message">Message received.</param>
        public TransportProviderMessageReceivedEventArgs(Messages.Message message)
        {
            MessageReceived = message;
        }
        
        #endregion
    }

    #region Delegates

    /// <summary>
    /// Delegate for the SPI Transport Provider message received event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">TransportProviderMessageReceivedEventArgs containing information about the message that has been received.</param>
    public delegate void TransportProviderMessageReceivedEventHandler(object sender, TransportProviderMessageReceivedEventArgs e);
    
    #endregion
    
}
