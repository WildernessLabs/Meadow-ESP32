using System;
using System.Collections.Generic;
using System.Text;

namespace Meadow.Core.Esp32
{
    /// <summary>
    ///     This class deals with the responses from the ESP32 indicating
    ///     success or failure of a request to send a packet of data to
    ///     the ESP32.
    /// </summary>
    public class TransportProviderEventArgs
    {
        #region Properties

        /// <summary>
        ///     Enum listing possible error that can occur.
        /// </summary>
        public enum TransportPacketType { Unknown, ACK, NAK, Timeout }

        /// <summary>
        ///     PacketType code returned by the Uart.
        /// </summary>
        public StatusCodes StatusCode { get; private set; }

        /// <summary>
        ///     Indicate the error that has occurred.
        /// </summary>
        public TransportPacketType PacketType { get; private set; }

        #endregion Properties

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        public TransportProviderEventArgs()
        {
        }

        /// <summary>
        ///     Create a new UartTransportEventArgs indicating that the specified error has occurred.
        /// </summary>
        /// <param name="packetType">PacketType that has occurred.</param>
        /// <param name="statusCode">Status code returned by the Uart.</param>
        public TransportProviderEventArgs(TransportPacketType packetType, StatusCodes statusCode)
        {
            PacketType = packetType;
            StatusCode = statusCode;
        }

        #endregion
    }

    #region Delegates

    /// <summary>
    ///     Delegate for the Uart error event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">UartTransportEventArgs indicating the error condition that occurred.</param>
    public delegate void TransportProviderErrorEventHandler(object sender, TransportProviderEventArgs e);

    #endregion
}
