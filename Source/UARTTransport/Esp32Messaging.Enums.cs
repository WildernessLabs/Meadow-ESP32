using System;

namespace Esp32Messaging
{
    /// <summary>
    ///     Access point authentication method.
    /// </summary>
    public enum WiFiAuthenticationMode : byte
    {
        Open = 0,
        Wep = 1,
        WpaPsk = 2,
        Wpa2Psk = 3,
        WpaWpa2Psk = 4,
        Wpa2Enterprise = 5
    }

    public enum WiFiCountryPolicy
    {
        Automatic = 0,
        Manual = 1
    }

    /// <summary>
    ///     Location of the secondary channel in respect to the primary channel.
    /// </summary>
    public enum WiFiSecondChannel
    {
        None = 0,
        Above = 1,
        Below = 2
    }

    /// <summary>
    ///     WiFi scan type.
    /// </summary>
    public enum WiFiScanType
    {
        Active = 0,
        Passive = 1
    }

    /// <summary>
    ///     Encryption method used by the access point.
    /// </summary>
    public enum WiFiCipherType
    {
        None = 0,
        Wep40 = 1,
        Wep104 = 2,
        Tkip = 3,
        Ccmp = 4,
        TkipCcmp = 5,
        Unknown = 6
    }

    public enum Antenna
    {
        Antenna0 = 0,
        Antenna1 = 1
    }

    /// <summary>
    ///     Errors that could be reported by the Uart.
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
        ///     Uart as requested that the message is restarted from
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
