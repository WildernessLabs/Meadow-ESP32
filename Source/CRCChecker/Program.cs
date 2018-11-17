using System;
using System.Collections;

namespace CRCChecker
{
    class Program
    {
        private static ArrayList bytes = new ArrayList();

        /// <summary>
        ///     Seed for the CRC32 algorithm.
        /// </summary>
        private const UInt32 CRC32_SEED = 0xffffffff;

        /// <summary>
        ///     Progressively calculate the CRC32 value.
        /// </summary>
        /// <remarks>
        ///     This allows method allows the CRC32 value to be built up as the data bytes
        ///     from the serial port are received.  This means that all of the data frame
        ///     does not need to be present in order to calculate this value.
        /// </remarks>
        /// <param name="currentChecksum">Current value for the _originalChecksum.</param>
        /// <param name="data">ApplicationData byte to be processed.</param>
        /// <returns>Next value for the CRC32.</returns>
        private static UInt32 ProgressiveCRC32(UInt32 currentChecksum, byte data)
        {
            UInt32 crc = currentChecksum;
            crc ^= data;
            for (UInt32 index = 0; index < 8; index++)
            {
                UInt32 mask = (UInt32) (-(crc & 0x01));
                crc = (crc >> 1) ^ (0xedb88320 & mask);
            }
            return (crc);
        }

        /// <summary>
        ///     Calculate the 32-bit CRC for the array of bytes.
        /// </summary>
        /// <param name="data">Buffer of data.</param>
        /// <returns>32-bit CRC for the array of bytes.</returns>
        private static UInt32 CRC32(byte[] data)
        {
            UInt32 crc = CRC32_SEED;

            for (int index = 0; index < data.Length; index++)
            {
                crc = ProgressiveCRC32(crc, data[index]);
            }

            return (crc);
        }

        static void Main(string[] args)
        {
            Console.WriteLine("CRC Checker");
            Console.WriteLine("Enter a sequence of bytes terminating with a blank line:");
            string line = Console.ReadLine();
            while (line != "")
            {
                string hexadecimal = "0123456789ABCDEF";
                
                int value = 0;
                value = (16 * hexadecimal.IndexOf(line.Substring(0, 1).ToUpper()));
                value += hexadecimal.IndexOf(line.Substring(1, 1).ToUpper());
                bytes.Add((byte) (value & 0xff));
                line = Console.ReadLine();
            }
            byte[] byteValues = new byte[bytes.Count];
            for (int index = 0; index < bytes.Count; index++)
            {
                byteValues[index] = (byte) (((byte) bytes[index]) & 0xff);
            }
            uint crc = CRC32(byteValues);
            Console.Write("CRC value: ");
            for (int index = 0; index < 4; index++)
            {
                byte b = (byte) ((crc & (0xff << (8 * index))) >> (8 * index));
                Console.Write($" {b:X2}");
            }
            Console.WriteLine();
        }
    }
}
