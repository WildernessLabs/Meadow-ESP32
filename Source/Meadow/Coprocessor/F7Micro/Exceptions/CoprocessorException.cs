using System;

namespace Meadow.Coprocessor.F7Micro.Exceptions
{
    /// <summary>
    /// Generic coprocessor exception thrown when the coprocessor returns an error
    /// code that is not dealt with through error codes (such as errno).
    /// </summary>
    public class CoprocessorException : Exception
    {
        #region Properties
        
        /// <summary>
        /// Status / error code returned by the coprocessor.
        /// </summary>
        public StatusCodes StatusCode { get; private set; }
        
        #endregion Properties
        
        #region Constructors
        
        /// <summary>
        /// Default constructor is private to prevent it from being used.
        /// </summary>
        private CoprocessorException()
        {
        }

        /// <summary>
        /// Create a new CoprocessorResponseNotFoundException object passing on the
        /// message information.
        /// </summary>
        /// <param name="message">Message for the calling application.</param>
        public CoprocessorException(string message)
            : base(message)
        {
            StatusCode = StatusCodes.CompletedOK;
        }
        
        /// <summary>
        /// Create a new CoprocessorException object passing on the error code in a string message.
        /// </summary>
        /// <param name="errorCode">Error number for the calling application.</param>
        public CoprocessorException(StatusCodes errorCode)
            : base($"Coprocessors returned error {errorCode}")
        {
            StatusCode = errorCode;
        }

        /// <summary>
        /// Create a new CoprocessorResponseNotFoundException object passing on the
        /// message and inner exception details.
        /// </summary>
        /// <param name="message">Message for the calling application.</param>
        /// <param name="inner">Inner exception information.</param>
        public CoprocessorException(string message, Exception inner)
            : base(message, inner)
        {
            StatusCode = StatusCodes.CompletedOK;
        }
        
        #endregion Constructors
    }
}
