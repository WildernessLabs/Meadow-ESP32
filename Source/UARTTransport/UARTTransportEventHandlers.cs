using System;
using System.Collections.Generic;
using System.Text;

namespace UARTTransport
{
    /// <summary>
    ///     This class deals with the responses from the ESP32 indicating
    ///     success or failure of a request to send a packet of data to
    ///     the ESP32.
    /// </summary>
    public class UARTResponseEventArgs
    {
        #region Properties

        /// <summary>
        ///     Enum listing possible error that can occur.
        /// </summary>
        public enum ErrorType {  Unknown, ACK, NAK, Timeout }

        /// <summary>
        ///     Indicate the error that has occurred.
        /// </summary>
        public ErrorType Error { get; private set; }

        #endregion Properties

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        public UARTResponseEventArgs()
        {
        }

        /// <summary>
        ///     Create a new UARTResponseEventArgs indicating that the specified error has occurred.
        /// </summary>
        /// <param name="errorType">Error that has occurred.</param>
        public UARTResponseEventArgs(ErrorType errorType)
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
    /// <param name="e">UARTResponseEventArgs indicating the error condition that occurred.</param>
    public delegate void UARTResponseEventHandler(object sender, UARTResponseEventArgs e);

    #endregion
}
