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
        public enum TransportPacketType { Unknown, ACK, NAK, Timeout }

        /// <summary>
        ///     Packet code returned by the UART.
        /// </summary>
        public UInt32 StatusCode { get; private set; }

        /// <summary>
        ///     Indicate the error that has occurred.
        /// </summary>
        public TransportPacketType Packet { get; private set; }

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
        /// <param name="packetType">Packet that has occurred.</param>
        /// <param name="statusCode">Status code returned by the UART.</param>
        public UARTResponseEventArgs(TransportPacketType packetType, UInt32 statusCode)
        {
            Packet = packetType;
            StatusCode = statusCode;
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
