using System;

namespace Meadow.Core.Esp32
{
    public class EncodersBase
    {
        /// <summary>
        ///     Take two bytes from the buffer and encode them as a 16-bit integer.
        ///     Note that the data should be encoded as LSB first.
        /// </summary>
        /// <param name="buffer">Buffer holding the value.</param>
        /// <param name="offset">Offset into the buffer for the LSB.</param>
        public static UInt16 ExtractUInt16(byte[] buffer, int offset)
        {
            UInt16 result;

            result = buffer[offset];
            result |= (UInt16) (buffer[offset + 1] << 8);
            return (result);
        }

        /// <summary>
        ///     Encode a 16-bit integer as two bytes in the buffer.  The data is
        ///     encoded LSB first.
        /// </summary>
        /// <param name="value">Value to encode.</param>
        /// <param name="buffer">Buffer to store the value.</param>
        /// <param name="offset">Offset into the buffer for the first byte to store.</param>
        public static void EncodeUInt16(UInt16 value, byte[] buffer, int offset)
        {
            buffer[offset] = (byte) (value & 0xff);
            buffer[offset + 1] = (byte) ((value >> 8) & 0xff);
        }

        /// <summary>
        ///     Take four bytes from the buffer and encode them as a 32-bit integer.
        ///     Note that the data should be encoded as LSB first.
        /// </summary>
        /// <param name="buffer">Buffer holding the 32-bit value to decode.</param>
        /// <param name="offset">Offset of the 32-bit in the buffer.</param>
        public static UInt32 ExtractUInt32(byte[] buffer, int offset)
        {
            UInt32 result;

            result = buffer[offset];
            result |= (UInt32) (buffer[offset + 1] << 8);
            result |= (UInt32) (buffer[offset + 2] << 16);
            result |= (UInt32) (buffer[offset + 3] << 24);
            return (result);
        }

        /// <summary>
        ///     Encode a 32-bit integer as four bytes in the buffer.  The data is
        ///     encoded LSB first.
        /// </summary>
        /// <param name="value">Value to encode.</param>
        /// <param name="buffer">Buffer to store the value.</param>
        /// <param name="offset">Offset into the buffer for the first byte to store.</param>
        public static void EncodeUInt32(UInt32 value, byte[] buffer, int offset)
        {
            buffer[offset] = (byte) (value & 0xff);
            buffer[offset + 1] = (byte) ((value >> 8) & 0xff);
            buffer[offset + 2] = (byte) ((value >> 16) & 0xff);
            buffer[offset + 3] = (byte) ((value >> 24) & 0xff);
        }

        /// <summary>
        ///     Encode a string into the buffer as a stream of bytes.
        /// </summary>
        /// <remarks>
        ///     An additional null terminating byte is added to the end of the string.
        /// </remarks>
        /// <param name="str">String to encode.</param>
        /// <param name="buffer">Buffer to hold the characters.</param>
        /// <param name="offset">Offset into the buffer for the first character.</param>
        public static void EncodeString(string str, byte[] buffer, int offset)
        {
            Array.Copy(str.ToCharArray(), 0, buffer, offset, str.Length);
            buffer[offset + str.Length + 1] = 0;
        }

        /// <summary>
        ///     Extract a string from the buffer.
        /// </summary>
        /// <remarks>
        ///     Extraction will start offset bytes into the buffer and will
        ///     stop when a 0 (C string terminator) byte in encountered.
        /// </remarks>
        /// <returns>The string.</returns>
        /// <param name="buffer">Buffer holding the data to be decoded.</param>
        /// <param name="offset">Offset into the buffer of the string.</param>
        public static string ExtractString(byte[] buffer, int offset)
        {
            string result = "";

            while (buffer[offset] != 0)
            {
                result += buffer[offset];
                offset++;
            }
            return (result);
        }
    }
}