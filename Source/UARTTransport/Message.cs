using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UARTTransport;

namespace Esp32Messaging
{
    public class Message : IEnumerable
    {
        #region Constants

        /// <summary>
        ///     Location of the Interface indicator in the message header.
        /// </summary>
        public const int INTERFACE = 0;

        /// <summary>
        ///     Location of the Function bytes (4) in the message header.
        /// </summary>
        public const int FUNCTION_NUMBER = INTERFACE + 1;

        /// <summary>
        ///     Location of the MessageID bytes (4) in the message header
        /// </summary>
        public const int MESSAGE_ID = FUNCTION_NUMBER + 4;

        /// <summary>
        ///     Location of the Payload length bytes (2) in the message header.
        /// </summary>
        public const short PAYLOAD_LENGTH = MESSAGE_ID + 4;

        /// <summary>
        ///     Length of the header in bytes.
        /// </summary>
        public const int HEADER_LENGTH = PAYLOAD_LENGTH + 2;

        /// <summary>
        ///     Maximum length of the payload of a message.
        /// </summary>
        public const UInt16 MAXIMUM_MESSAGE_LENGTH = 0xffff;

        #endregion Constants

        #region Private member variables

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
        public UInt32 MessageID { get; private set; }

        /// <summary>
        ///     Payload of the message (data for the function to act upon).
        /// </summary>
        private byte[] Payload { get; set; }

        #endregion Private member variables

        #region IEnumerable interface implementation

        public IEnumerator GetEnumerator()
        {
            byte sequenceNumber = 0;
            Frame frame;
            byte[] buffer = new byte[HEADER_LENGTH];
            //
            //  Return the message header.
            //
            buffer[INTERFACE] = Device;
            Encoders.EncodeUInt32(Function, buffer, FUNCTION_NUMBER);
            Encoders.EncodeUInt32(MessageID, buffer, MESSAGE_ID);
            Encoders.EncodeUInt16((UInt16) (Payload.Length & 0xffff), buffer, PAYLOAD_LENGTH);
            frame = new Frame(UART.FrameType.ApplicationHeader, MessageID, sequenceNumber++, buffer);
            yield return frame;
            //
            //  Now we can process the message payload.
            //
            UInt16 start = 0;
            int amountLeftToSend = Payload.Length;
            while (amountLeftToSend > 0)
            {
                byte length = (byte) ((amountLeftToSend < UART.MAXIMUM_PAYLOAD_LENGTH ? amountLeftToSend : UART.MAXIMUM_PAYLOAD_LENGTH) & 0xff);
                if (length != buffer.Length)
                {
                    buffer = new byte[length];
                }
                Array.Copy(Payload, start, buffer, 0, length);
                start += length;
                amountLeftToSend -= length;
                frame = new Frame(UART.FrameType.ApplicationData, MessageID, sequenceNumber++, buffer);
                yield return frame;
            }
        }

        #endregion IEnumerable interface implementation

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
        /// <param name="payload">Payload for this message.</param>
        public Message(byte device, UInt32 function, UInt32 messageID, byte[] payload)
        {
            if (payload.Length > MAXIMUM_MESSAGE_LENGTH)
            {
                throw new ArgumentOutOfRangeException(nameof(payload), $"Payload length should be less than or equal to {MAXIMUM_MESSAGE_LENGTH} bytes");
            }

            Device = device;
            Function = function;
            MessageID = messageID;
            Payload = payload;
        }

        #endregion Constructors

        #region Methods


        #endregion Methods
    }
}
