using System;
using System.Text;
using System.Collections;
using System.Threading;

namespace DebugHelpers
{
    public static class DebugHelpers
    {
        #region Constants
        
        /// <summary>
        /// Header string for a 16-byte buffer plus address prefix.
        /// </summary>
        private const string HEXADECIMAL_BUFFER_HEADER = "             0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f";
        
        #endregion Constants
        
        #region Methods
        
        /// <summary>
        ///     Convert a byte array to a series of hexadecimal numbers
        ///     separated by a minus sign.
        /// </summary>
        /// <param name="bytes">Array of bytes to convert.</param>
        /// <returns>series of hexadecimal bytes in the format xx-yy-zz</returns>
        public static string Hexadecimal(byte[] bytes)
        {
            string result = string.Empty;

            for (byte index = 0; index < bytes.Length; index++)
            {
                if (index > 0)
                {
                    result += "-";
                }
                result += HexadecimalDigits(bytes[index]);
            }

            return (result);
        }

        /// <summary>
        ///     Convert a byte into the hex representation of the value.
        /// </summary>
        /// <param name="b">Value to convert.</param>
        /// <returns>Two hexadecimal digits representing the byte.</returns>
        private static string HexadecimalDigits(byte b)
        {
            char[] digits = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
            return "" + digits[b >> 4] + digits[b & 0xf];
        }

        /// <summary>
        ///     Convert a byte into hexadecimal including the "0x" prefix.
        /// </summary>
        /// <param name="b">Value to convert.</param>
        /// <returns>Hexadecimal string including the 0x prefix.</returns>
        public static string Hexadecimal(byte b)
        {
            return "0x" + HexadecimalDigits(b);
        }

        /// <summary>
        ///     Convert an unsigned short into hexadecimal.
        /// </summary>
        /// <param name="us">Unsigned short value to convert.</param>
        /// <returns>Hexadecimal representation of the unsigned short.</returns>
        public static string Hexadecimal(ushort us)
        {
            return "0x" + HexadecimalDigits((byte)((us >> 8) & 0xff)) + HexadecimalDigits((byte)(us & 0xff));
        }

        /// <summary>
        ///     Convert an integer into hexadecimal.
        /// </summary>
        /// <param name="i">Integer to convert to hexadecimal.</param>
        /// <returns>Hexadecimal representation of the unsigned short.</returns>
        private static string Hexadecimal(int i)
        {
            return "0x" + HexadecimalDigits((byte)((i >> 24) & 0xff)) + HexadecimalDigits((byte)((i >> 16) & 0xff)) +
                   HexadecimalDigits((byte)((i >> 8) & 0xff)) + HexadecimalDigits((byte)(i & 0xff));
        }

        /// <summary>
        /// Generate a single line or printable output from a buffer.
        /// </summary>
        /// <param name="address">Offset into the buffer to start processing.</param>
        /// <param name="buffer">Buffer to be used as a source of data.</param>
        /// <returns>String containing the hex address and up to 16 bytes of data from the buffer.</returns>
        private static string PrintableBufferLine(int address, byte[] buffer)
        {
            int end = address + 16;
            StringBuilder result = new StringBuilder(HEXADECIMAL_BUFFER_HEADER.Length);

            if (buffer.Length <= end)
            {
                end = buffer.Length;
            }
            result.Append(Hexadecimal(address));
            result.Append(": ");
            for (var index = address; index < end; index++)
            {
                result.Append(Hexadecimal(buffer[index]));
                result.Append(" ");
            }

            return (result.ToString());
        }

        /// <summary>
        /// Convert the array of bytes into an ArrayList of strings that can be used to display a buffer
        /// in hexadecimal.
        /// </summary>
        /// <param name="buffer">Byte array of the buffer to be converted to printable format.</param>
        /// <returns>ArrayList of printable strings, one per line.</returns>
        public static ArrayList PrintableBuffer(byte[] buffer)
        {
            ArrayList result = new ArrayList();
            result.Add(HEXADECIMAL_BUFFER_HEADER);
            for (var index = 0; index < buffer.Length; index += 16)
            {
                result.Add(PrintableBufferLine(index, buffer));
            }
            return(result);
        }

        /// <summary>
        /// Display a timestamped message on the console.
        /// </summary>
        /// <param name="message">Message to be displayed on the console.</param>
        /// <param name="showOutput">Determine if the method should show the message.</param>
        public static void LogMessage(bool showOutput, string message)
        {
            #if DEBUG
            if (showOutput)
            {
                Console.WriteLine($"STM: {DateTime.Now.ToString("HH:mm:ss")}: {message}");
                Thread.Sleep(5);
            }
            #endif
        }

        /// <summary>
        /// Display a timestamped list of string held in an array on the console.
        /// </summary>
        /// <param name="messages">Array of strings to be displayed.</param>
        /// <param name="showOutput">Determine if the method should show the message.</param>
        public static void LogMessage(bool showOutput, ArrayList lines)
        {
            #if DEBUG
            if (showOutput)
            {
                foreach (string s in lines)
                {
                    LogMessage(showOutput, s);
                }
            }
            #endif
        }
        
        #endregion Methods
    }
}
