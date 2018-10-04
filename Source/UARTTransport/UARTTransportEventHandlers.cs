using System;
using System.Collections.Generic;
using System.Text;

namespace UARTTransport
{
    public class UARTErrorEventArgs
    {
        /// <summary>
        ///     Enum listing possible error that can occur.
        /// </summary>
        public enum ErrorType {  NAK, Timeout }

        /// <summary>
        ///     Indicate the error that has occurred.
        /// </summary>
        public ErrorType Error { get; private set; }

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        public UARTErrorEventArgs()
        {
        }

        /// <summary>
        ///     Create a new UARTErrorEventArgs indicating that the specified error has occurred.
        /// </summary>
        /// <param name="errorType">Error that has occurred.</param>
        public UARTErrorEventArgs(ErrorType errorType)
        {
            Error = errorType;
        }

        #endregion
    }

    #region Delegates

    /// <summary>
    ///     Delegate for the UART error event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">UARTErrorEventArgs indicating the error condition that occurred.</param>
    public delegate void UARTErrorEventHandler(object sender, UARTErrorEventArgs e);

    #endregion
}
