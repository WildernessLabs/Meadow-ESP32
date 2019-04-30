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
        }

        public const UInt32 HEADER_SIZE = 23;
        public const int CRC_OFFSET = 18;
    }
}
