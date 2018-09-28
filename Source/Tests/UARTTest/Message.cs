using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UARTTest
{
    class Message
    {
        #region Constants

        public const int INTERFACE = 0;
        public const int FUNCTION_NUMBER = INTERFACE + 1;
        public const int MESSAGE_ID = FUNCTION_NUMBER + 4;
        public const short PACKET_LENGTH = MESSAGE_ID + 4;
        public const int HEADER_LENGTH = PACKET_LENGTH + 2;

        #endregion Constants

        #region Methods

        public static byte[] CreateHeader(byte device, UInt32 function, UInt32 messageID)
        {
            byte[] buffer = new byte[HEADER_LENGTH + 6];

            buffer[INTERFACE] = device;
            Encoders.EncodeUInt32(buffer, FUNCTION_NUMBER, function);
            Encoders.EncodeUInt32(buffer, MESSAGE_ID, messageID);
            Encoders.EncodeUInt16(buffer, PACKET_LENGTH, 45);

            return (buffer);
        }

        #endregion Methods
    }
}
