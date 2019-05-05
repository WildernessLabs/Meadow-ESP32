using System;

namespace Meadow.Core.Esp32
{
    /// <summary>
    /// Base class for any hardware transport providers.
    /// </summary>
    public abstract class TransportProviderBase
    {
        #region Properties
        
        /// <summary>
        /// Maximum number of times the system will attempt to retry sending a
        /// failed message to the ESp32.
        /// </summary>
        public int MaximumRetryCount { get; set; } = 3;
        
        #endregion
        
        #region Delegates and events
        
        /// <summary>
        /// Event that will be raised when an error occurs when sending
        /// a message to the ESP32.
        /// </summary>
        public abstract event TransportProviderErrorEventHandler TransportError;
        
        /// <summary>
        /// Event that will be raised when a message is received and correctly
        /// decoded.
        /// </summary>
        public abstract event TransportProviderDataReceivedEventHandler MessageReceived;
        
        #endregion
        
        #region Methods

        /// <summary>
        /// Add a message to the queue of messages waiting to be sent to the ESP32.
        /// </summary>
        /// <param name="message">Message to add to the queue.</param>
        public abstract void QueueMessageForEsp32(Messages.Message message);
        
        #endregion
    }
}
