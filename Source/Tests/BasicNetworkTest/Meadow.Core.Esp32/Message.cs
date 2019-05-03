using System;
namespace Meadow.Core.Esp32
{
    public partial class Messages
    {
        /// <summary>
        ///     The Message structure is the main method of sending data between the STM32
        ///     and the ESP32.
        /// </summary>
        /// <remarks>
        ///     Unlike the rest of the messages this structure plus the encode/extract
        ///     methods are haded coded and not generated automatically.
        /// </remarks>
        public struct Message
        {
            public byte MessageType;
            public byte Interface;
            public UInt32 Function;
            public UInt32 StatusCode;
            public UInt32 MessageID;
            public UInt32 PayloadLength;
            public UInt32 Crc;
            public byte[] Payload;

            /// <summary>
            /// Constructor for a Message.
            /// </summary>
            /// <param name="messageType">Type of message to be sent <see cref="PacketType" />.</param>
            /// <param name="esp32Interface">Interface on the ESP32 that this message is destined for (see <see cref="Esp32Interfaces" />.</param>
            /// <param name="function">Function to be executed.</param>
            /// <param name="messageId">Message ID for this message.</param>
            /// <param name="statusCode">Status code for result messages.</param>
            /// <param name="payload">Payload associated with this message.</param>
            public Message(PacketType messageType, Esp32Interfaces esp32Interface, UInt32 function, UInt32 messageId, UInt32 statusCode, byte[] payload)
            {
                MessageType = (byte) messageType;
                Interface = (byte) esp32Interface;
                Function = function;
                MessageID = messageId;
                StatusCode = statusCode;
                if (payload == null)
                {
                    Payload = null;
                    PayloadLength = 0;
                }
                else
                {
                    Payload = payload;
                    PayloadLength = (UInt32) payload.Length;
                }
                Crc = 0;
            }
        }

        /// <summary>
        /// Size of an encoded header (in bytes).
        /// </summary>
        public const UInt32 HEADER_SIZE = 23;

        /// <summary>
        /// Offset of the CRC in an encoded message header.
        /// </summary>
        public const int CRC_OFFSET = 18;

        /// <summary>
        /// Message ID used to indicate an invalid (or unknown) message ID.
        /// </summary>
        public const UInt32 INVALID_MESSAGE_ID = 0xffffffff;

        /// <summary>
        /// Invalid function number.
        /// </summary>
        public const UInt32 INVALID_FUNCTION = 0xffffffff;
    }
}
