using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UARTTransport;

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

        /// <summary>
        ///     Device / interface this message is destined for.
        /// </summary>
        private byte Device { get; set; }

        /// <summary>
        ///     Function to be executed by the device.
        /// </summary>
        private UInt32 Function { get; set; }
        
        /// <summary>
        ///     Unique message ID.
        /// </summary>
        private UInt32 MessageID { get; set; }

        /// <summary>
        ///     Payload of the message (data for the function to act upon).
        /// </summary>
        private byte[] Payload { get; set; }

        #region Constructors

        private Message()
        {
        }

        /// <summary>
        ///     Construct a new message for the ESP32.
        /// </summary>
        /// <param name="device">Device / Interface the message is intended for.</param>
        /// <param name="function">Function that the device / interface should perform.</param>
        /// <param name="messageID">Unique ID of this message.</param>
        public Message(byte device, UInt32 function, UInt32 messageID, byte[] payload)
        {
            Device = device;
            Function = function;
            MessageID = messageID;
            Payload = payload;
        }

        #endregion Constructors

        #region Methods

        public static byte[] CreateHeader(byte device, UInt32 function, UInt32 messageID)
        {
            byte[] buffer = new byte[HEADER_LENGTH + 6];

            buffer[INTERFACE] = device;
            //Encoders.EncodeUInt32(buffer, FUNCTION_NUMBER, function);
            //Encoders.EncodeUInt32(buffer, MESSAGE_ID, messageID);
            //Encoders.EncodeUInt16(buffer, PACKET_LENGTH, 45);

            return (buffer);
        }

        public static byte[] CreateDataPacket(byte[] payload)
        {
            byte[] result = new byte[10];
            return (result);
        }

        public static byte[] CreateDataPacket(byte[] payload, int offset, int length)
        {
            byte[] result = new byte[10];
            return (result);
        }

        /// <summary>
        ///     Send this message on the specified UART.
        /// </summary>
        /// <param name="uart">UART object used to send the message.</param>
        public void Send(UART uart)
        {

        }

        #endregion Methods


    }
}
