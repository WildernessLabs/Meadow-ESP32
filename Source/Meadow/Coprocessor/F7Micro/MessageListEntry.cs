using System;
using System.Threading;

namespace Meadow.Coprocessor.F7Micro
{
    /// <summary>
    /// MessageListEntry objects are used to record the messages IDs that have been
    /// used in messages that have been sent to the coprocessor.
    /// </summary>
    public class MessageListEntry
    {
        #region Properties
        
        /// <summary>
        /// Wait handle used to hold the caller and used by the message dispatcher to
        /// waken the caller.
        /// </summary>
        public EventWaitHandle WaitHandle { get; private set; }

        /// <summary>
        /// Response from the coprocessor.
        /// </summary>
        public Messages.Message? Response { get; set; }
        
        #endregion Properties
        
        #region Constructrors

        /// <summary>
        /// Default constructor (private to prevent it from being used).
        /// </summary>
        private MessageListEntry()
        {
        }

        /// <summary>
        /// Create a new MessageListEntry object recording the message ID and the
        /// handle used to wake the caller.
        /// </summary>
        /// <param name="waitHandle">Handle that can be used to inform (awaken) the caller that a response is ready.</param>
        public MessageListEntry(EventWaitHandle waitHandle)
        {
            WaitHandle = waitHandle;
            Response = null;
        }

        #endregion Constructors
    }
}
