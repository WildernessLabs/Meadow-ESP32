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
        /// ESP32 Interface that generated the error.
        /// </summary>
        public Esp32Interfaces Interface;

        /// <summary>
        /// ID of the message that generated the error.
        /// </summary>
        public UInt32 MessageId;

        /// <summary>
        ///     PacketType code returned by the Uart.
        /// </summary>
        public StatusCodes StatusCode { get; private set; }

        #endregion Properties

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        private TransportProviderEventArgs()
        {
        }

        /// <summary>
        ///     Create a new UartTransportEventArgs indicating that the specified error has occurred.
        /// </summary>
        /// <param name="esp32Interface">Interface on the ESP32 that caused the problem.</param>
        /// <param name="messageId">ID of the message that caused the error.</param>
        /// <param name="statusCode">Status code returned by the Uart.</param>
        public TransportProviderEventArgs(Esp32Interfaces esp32Interface, UInt32 messageId, StatusCodes statusCode)
        {
            Interface = esp32Interface;
            MessageId = messageId;
            StatusCode = statusCode;
        }

        #endregion
    }

    #region Delegates

    /// <summary>
    ///     Delegate for the SPI Transport Provider error event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">TransportProviderEventArgs indicating the error condition that occurred.</param>
    public delegate void TransportProviderErrorEventHandler(object sender, TransportProviderEventArgs e);

    #endregion
}
