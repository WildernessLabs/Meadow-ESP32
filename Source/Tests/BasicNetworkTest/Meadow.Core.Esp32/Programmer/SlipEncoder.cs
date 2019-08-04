namespace Meadow.Core.Esp32
{
    /// <summary>
    /// Provide static methods to encode / decode SLIP data.
    /// </summary>
    /// <remarks>
    /// SLIP encoding uses special bytes to indicate the start and end of a packet.  These bytes
    /// are set to 0xC0.  This requires that any instance of 0xC0 in the data stream are replaced
    /// by a padding character and a special character which represents 0xC0.
    ///
    /// This in turn requires that the padding character is also replaced by an encoding sequence.
    ///
    /// So:
    ///     Add 0xC0 to the start and end of the data stream.
    ///     Convert all instances of 0xC0 in the data stream to 0xDB 0xDC
    ///     Convert all instances of 0xDB in the data stream to 0xDB 0xDD
    /// </remarks>
    public static class SlipEncoder
    {
        #region Methods

        /// <summary>
        /// Decode an array of bytes encoded using the SLIP protocol.
        /// </summary>
        /// <param name="data">Array of bytes that is encoded using the SLIP protocol.</param>
        /// <returns>Decoded array of bytes.</returns>
        public static byte[] Decode(byte[] data)
        {
            int decodedArrayLength = data.Length;
            foreach (byte b in data)
            {
                if ((b == 0xC0) || (b == 0xDB))
                    decodedArrayLength--;
            }
            byte[] result = new byte[decodedArrayLength];
            int currentDestinationByte = 0;
            int currentSourceByte = 0;
            while (currentSourceByte < data.Length)
            {
                switch (data[currentSourceByte])
                {
                    case 0xC0:
                        //
                        //    This should only occur as the first and last byte as they
                        //    will be encoded elsewhere in the data stream so ignore
                        //    this value.
                        //
                        break;
                    case 0xDB:
                        currentSourceByte++;
                        if (data[currentSourceByte] == 0xDC)
                        {
                            result[currentDestinationByte] = 0xC0;
                        }
                        else
                        {
                            result[currentDestinationByte] = 0xDB;
                        }
                        break;
                    default:
                        result[currentDestinationByte] = data[currentSourceByte];
                        break;
                }
                currentSourceByte++;
                currentDestinationByte++;
            }
            return (result);
        }

        /// <summary>
        /// Encode an array of bytes using the SLIP protocol.
        /// </summary>
        /// <param name="data">Array of bytes to be encoded.</param>
        /// <returns>Array of SLIP encoded bytes.</returns>
        public static byte[] Encode(byte[] data)
        {
            int encodedArrayLength = data.Length + 2;
            foreach (byte b in data)
            {
                if ((b == 0xC0) || (b == 0xDB))
                    encodedArrayLength++;
            }

            byte[] result = new byte[encodedArrayLength];
            int currentByte = 0;
            result[currentByte++] = 0xC0;
            foreach (byte b in data)
            {
                switch (b)
                {
                    case 0xC0:
                        result[currentByte++] = 0xDB;
                        result[currentByte++] = 0xDC;
                        break;
                    case 0xDB:
                        result[currentByte++] = 0xDB;
                        result[currentByte++] = 0xDD;
                        break;
                    default:
                        result[currentByte++] = b;
                        break;
                }
            }
            result[currentByte] = 0xC0;

            return (result);
        }

        #endregion
    }
}
