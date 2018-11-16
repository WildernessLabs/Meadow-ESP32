using System;
using System.Diagnostics;

namespace UARTTransport
{
    public class DebugHelpers
    {
        /// <summary>
        ///     Dump the array of bytes to the debug output in hexadecimal.
        /// </summary>
        /// <param name="buffer">Buffer to display.</param>
        /// <param name="amount">Optional amount of data to display.  If -1 then the full buffer will be displayed.</param>
        public static void DisplayBufferInHex(byte[] buffer, int amount = -1)
        {
            var line = string.Empty;
            Debug.Print("\n\n       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
            if (amount < 0)
            {
                amount = buffer.Length;
            }
            for (var index = 0; index < amount; index++)
            {
                if ((index % 16) == 0)
                {
                    if (line != string.Empty)
                    {
                        Debug.Print(line);
                        Debug.Print("\n");
                        line = String.Empty;
                    }
                    line += $"{index:X4}: ";
                }
                line += $"{buffer[index]:X2} ";
            }
            if (line != string.Empty)
            {
                Debug.Print(line);
            }

            Debug.Print("\n");
        }
    }
}
