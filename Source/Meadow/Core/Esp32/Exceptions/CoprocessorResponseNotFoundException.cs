using System;

namespace Meadow.Core.Esp32.Exceptions
{
    /// <summary>
    /// Exception thrown when a response from the coprocessor could not be found
    /// in the list of responses stored.
    /// </summary>
    /// <remarks>
    /// This exception can be thrown in the following circumstances:
    ///     - A message ID could not be found in the list of messages that have
    ///       been submitted to the coprocessor.
    ///     - The message ID could be found but no response is available yet.
    /// </remarks>
    public class CoprocessorResponseNotFoundException : Exception
    {
        /// <summary>
        /// Default constructor for the CoprocessorResponseNotFoundException.
        /// </summary>
        public CoprocessorResponseNotFoundException()
        {
        }

        /// <summary>
        /// Create a new CoprocessorResponseNotFoundException object passing on the
        /// message information.
        /// </summary>
        /// <param name="message">Message for the calling application.</param>
        public CoprocessorResponseNotFoundException(string message)
            : base(message)
        {
        }

        /// <summary>
        /// Create a new CoprocessorResponseNotFoundException object passing on the
        /// message and inner exception details.
        /// </summary>
        /// <param name="message">Message for the calling application.</param>
        /// <param name="inner">Inner exception information.</param>
        public CoprocessorResponseNotFoundException(string message, Exception inner)
            : base(message, inner)
        {
        }
    }
}
