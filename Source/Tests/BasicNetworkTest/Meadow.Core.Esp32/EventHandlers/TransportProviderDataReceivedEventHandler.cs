using System;

namespace Meadow.Core.Esp32
{
    /// <summary>
    ///     Class holding the information about a frame that has been received
    ///     from the ESP32 via the Uart.
    /// </summary>
    public class TransportProviderDataReceivedEventArgs
    {
        #region Properties

        /// <summary>
        ///     Indicate the error that has occurred.
        /// </summary>
        public PacketType FrameType { get; private set; }

        /// <summary>
        ///     Message ID that this frame is part of.
        /// </summary>
        public UInt32 MessageID = 0;

        /// <summary>
        ///     Sequence number of this frame.
        /// </summary>
        public UInt16 SequenceNumber = 0;

        /// <summary>
        ///     Frame received from the Uart.
        /// </summary>
        public byte[] Frame = null;

        #endregion Properties

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        private TransportProviderDataReceivedEventArgs()
        {
        }

        /// <summary>
        ///     Create a new UARTDataReceivedEventArgs and add the data frame.
        /// </summary>
        /// <param name="frameType">Type of frame received.</param>
        /// <param name="messageID">Message ID used by the STM32 in the original request.</param>
        /// <param name="sequenceNumber">Sequence number of this frame in the response.</param>
        /// <param name="frame">Frame received from the Uart.</param>
        public TransportProviderDataReceivedEventArgs(PacketType frameType, UInt32 messageID, UInt16 sequenceNumber, byte[] frame)
        {
            FrameType = frameType;
            MessageID = messageID;
            SequenceNumber = sequenceNumber;
            Frame = frame;
        }

        #endregion
    }

    #region Delegates

    /// <summary>
    ///     Delegate for the Uart data received event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">UARTDataReceivedEventArgs containing the data that has been sent by the ESP32.</param>
    public delegate void TransportProviderDataReceivedEventHandler(object sender, TransportProviderDataReceivedEventArgs e);

    #endregion
}
