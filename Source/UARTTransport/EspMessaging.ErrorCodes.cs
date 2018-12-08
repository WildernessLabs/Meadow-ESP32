using System;
using System.Collections.Generic;
using System.Text;

namespace Esp32Messaging
{
    /// <summary>
    ///     Errors that could be reported by the UART.
    /// </summary>
    /// <remarks>
    ///     The error codes and values here must be kept in step
    ///     with the error codes in the ESP system.
    /// </remarks>
    public enum StatusCodes : UInt32
    {
        /// <summary>
        ///     No error, frame has been processed OK.
        /// </summary>
        OK = 0,

        /// <summary>
        ///     CRC check has failed.
        /// </summary>
        CRCError = 1,

        /// <summary>
        ///     UART as requested that the message is restarted from
        ///     from the start.
        /// </summary>
        Restart = 2,

        /// <summary>
        ///     General failure.
        /// </summary>
        Failure = 3,

        /// <summary>
        ///     Interface (device) not recognised.
        /// </summary>
        InvalidInterface = 4,

        /// <summary>
        ///     Error queuing the request.
        /// </summary>
        QueueError = 5,

        /// <summary>
        ///     PacketType / frame is invalid.
        /// </summary>
        InvalidPacket = 6,

        /// <summary>
        ///     Header for the message is invalid.
        /// </summary>
        InvalidHeader = 7,

        /// <summary>
        ///     Unexpected frame.
        /// </summary>
        UnexpectedData = 8,

        /// <summary>
        ///     End of frame marker is missing or invalid.
        /// </summary>
        MissingEndOfFrameMarker = 9
    }
}
