using System;
using UARTTransport;

namespace Esp32Messaging
{
    /// <summary>
    ///     ApplicationData frame for sending / receiving over the serial port.
    /// </summary>
    public class Frame
    {
        #region Properties.

        /// <summary>
        ///     Frame type.
        /// </summary>
        public UART.FrameType FrameType { get; private set; }

        /// <summary>
        ///     Message ID that this frame belongs to.
        /// </summary>
        public UInt32 MessageID { get; private set; }

        /// <summary>
        ///     Sequence number of this frame in the overall message.
        /// </summary>
        public byte SequenceNumber { get; private set; }

        /// <summary>
        ///     Payload bytes in this frame (maximum is 255).
        /// </summary>
        public byte[] Payload { get; private set; }

        #endregion Properties.

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        private Frame() { }

        /// <summary>
        ///     Create a new frame object.
        /// </summary>
        /// <param name="frameType">Frame type</param>
        /// <param name="messageID">ID of the message being sent.</param>
        /// <param name="sequenceNumber">Sequence number of this packet in the overall message.</param>
        /// <param name="payload">Payload bytes.</param>
        public Frame(UART.FrameType frameType, UInt32 messageID, byte sequenceNumber, byte[] payload)
        {
            FrameType = frameType;
            MessageID = messageID;
            SequenceNumber = sequenceNumber;
            Payload = payload;
        }

        #endregion Constructors
    }
}
