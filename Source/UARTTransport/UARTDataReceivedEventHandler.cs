using System;
using System.Collections.Generic;
using System.Text;
using System.Xml.Serialization;

namespace UARTTransport
{
    /// <summary>
    ///     Class holding the information about a frame that has been received
    ///     from the ESP32 via the UART.
    /// </summary>
    public class UARTDataReceivedEventArgs
    {
        #region Properties

        /// <summary>
        ///     Indicate the error that has occurred.
        /// </summary>
        public UART.FrameType FrameType { get; private set; }

        /// <summary>
        ///     Interface this message is intended for.
        /// </summary>
        public byte Interface = 0;

        /// <summary>
        ///     Message ID that this frame is part of.
        /// </summary>
        public UInt32 MessageID = 0;

        /// <summary>
        ///     Sequence number of this frame.
        /// </summary>
        public UInt16 SequenceNumber = 0;

        /// <summary>
        ///     Data received from the UART.
        /// </summary>
        public byte[] Data = null;

        #endregion Properties

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        public UARTDataReceivedEventArgs()
        {
        }

        /// <summary>
        ///     Create a new UARTDataReceivedEventArgs and add the data frame.
        /// </summary>
        /// <param name="device">Interface / device sending the data frame.</param>
        /// <param name="messageID">Message ID used by the STM32 in the original request.</param>
        /// <param name="sequenceNumber">Sequence number of this frame in the response.</param>
        /// <param name="frame">Data received from the UART.</param>
        public UARTDataReceivedEventArgs(byte device, UInt32 messageID, UInt16 sequenceNumber, byte[] frame)
        {
            Interface = device;
            MessageID = messageID;
            SequenceNumber = sequenceNumber;
            Data = frame;
        }

        #endregion
    }

    #region Delegates

    /// <summary>
    ///     Delegate for the UART data received event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">UARTDataReceivedEventArgs containing the data that has been sent by the ESP32.</param>
    public delegate void UARTDataReceivedEventHandler(object sender, UARTDataReceivedEventArgs e);

    #endregion
}
