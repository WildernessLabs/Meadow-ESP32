using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UARTTest
{
    class Message
    {
        public const int NUMBER_PATTERN_CHARACTERS = 3;

        public const byte START_OF_FRAME_CHARACTER = 0xaa;

        public const byte END_OF_FRAME_CHARACTER = 0x55;

        public const int INTERFACE = 0;
        public const int FUNCTION_BYTE0 = INTERFACE + 1;
        public const int FUNCTION_BYTE1 = FUNCTION_BYTE0 + 1;
        public const int FUNCTION_BYTE2 = FUNCTION_BYTE1 + 1;
        public const int FUNCTION_BYTE3 = FUNCTION_BYTE2 + 1;
        public const int MESSAGE_ID_BYTE0 = FUNCTION_BYTE3 + 1;
        public const int MESSAGE_ID_BYTE1 = MESSAGE_ID_BYTE0 + 1;
        public const int MESSAGE_ID_BYTE2 = MESSAGE_ID_BYTE1 + 1;
        public const int MESSAGE_ID_BYTE3 = MESSAGE_ID_BYTE2 + 1;
        public const short PACKET_LENGTH_BYTE0 = MESSAGE_ID_BYTE3 + 1;
        public const short PACKET_LENGTH_BYTE1 = PACKET_LENGTH_BYTE0 + 1;
        public const int HEADER_LENGTH = PACKET_LENGTH_BYTE1 + 1;

        private static void EncodeUInt16(byte[] buffer, int offset, UInt16 value)
        {
            buffer[offset] = (byte)(value & 0xff);
            buffer[offset + 1] = (byte)((value >> 8) & 0xff);
        }

        private static void EncodeUInt32(byte[] buffer, int offset, UInt32 value)
        {
            buffer[offset] = (byte)(value & 0xff);
            buffer[offset + 1] = (byte)((value >> 8) & 0xff);
            buffer[offset + 2] = (byte)((value >> 16) & 0xff);
            buffer[offset + 3] = (byte)((value >> 24) & 0xff);
        }

        public static byte[] CreateHeader(byte device, UInt32 function, UInt32 messageID)
        {
            byte[] buffer = new byte[HEADER_LENGTH + 6];

            buffer[0] = 0x81;
            buffer[1] = HEADER_LENGTH;
            buffer[INTERFACE + 2] = device;
            EncodeUInt32(buffer, FUNCTION_BYTE0 + 2, function);
            EncodeUInt32(buffer, MESSAGE_ID_BYTE0 + 2, messageID);
            EncodeUInt16(buffer, PACKET_LENGTH_BYTE0 + 2, 45);
            UInt32 crc = CRC32(buffer);
            EncodeUInt32(buffer, HEADER_LENGTH + 2, crc);

            return (buffer);
        }

        public static byte[] CreateDataPacket(byte[] message)
        {
            return (CreateDataPacket(message, 0, message.Length));
        }

        public static byte[] CreateDataPacket(byte[] message, int start, int length)
        {
            byte[] data = new byte[length + 6];
            data[0] = 0x82;
            data[1] = (byte) (length & 0xff);
            Array.Copy(message, start, data, 2, length);
            UInt32 crc = CRC32(data);
            EncodeUInt32(data, length + 2, crc);
            return (data);
        }
    }
}
