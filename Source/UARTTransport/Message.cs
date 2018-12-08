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
        ///     Location of the Payload length bytes (4) in the message header.
        /// </summary>
        public const short PAYLOAD_LENGTH = MESSAGE_ID + 4;

        /// <summary>
        ///     Length of the header in bytes.
        /// </summary>
        public const int HEADER_LENGTH = PAYLOAD_LENGTH + 4;

        /// <summary>
        ///     Maximum length of the payload of a message.
        /// </summary>
        public const UInt16 MAXIMUM_MESSAGE_LENGTH = 0xffff;

        /// <summary>
        ///     Location of the Frame type byte in a transport message.
        /// </summary>
        public const int TRANSPORT_FRAME_TYPE = 0;

        /// <summary>
        ///     Location of the status code byte in a transport frame.
        /// </summary>
        public const int TRANSPORT_STATUS_CODE = TRANSPORT_FRAME_TYPE + 1;

        /// <summary>
        ///     Transport message packet length.
        /// </summary>
        public const int TRANSPORT_MESSAGE_LENGTH = TRANSPORT_STATUS_CODE + 4;

        #endregion Constants

        #region Properties

        /// <summary>
        ///     Device / interface this message is destined for.
        /// </summary>
        public byte Device { get; private set; }

        /// <summary>
        ///     Function to be executed by the device.
        /// </summary>
        public UInt32 Function { get; private set; }
        
        /// <summary>
        ///     Unique message ID.
        /// </summary>
        public UInt32 MessageID { get; private set; }

        /// <summary>
        ///     Payload of the message (data for the function to act upon).
        /// </summary>
        public byte[] Payload { get; private set; }

        #endregion Properties

        #region IEnumerable interface implementation

        public IEnumerator GetEnumerator()
        {
            byte sequenceNumber = 0;
            byte[] buffer = new byte[HEADER_LENGTH];
            //
            //  Return the message header.
            //
            buffer[INTERFACE] = Device;
            Encoders.EncodeUInt32(Function, buffer, FUNCTION_NUMBER);
            Encoders.EncodeUInt32(MessageID, buffer, MESSAGE_ID);
            UInt16 amountLeftToSend = (UInt16) (Payload == null ? 0 : Payload.Length & 0xffff);
            Encoders.EncodeUInt16(amountLeftToSend, buffer, PAYLOAD_LENGTH);
            Frame frame = new Frame(UART.FrameType.ApplicationHeader, MessageID, sequenceNumber++, buffer);
            yield return frame;
            //
            //  Now we can process the message payload.
            //
            UInt16 start = 0;
            while (amountLeftToSend > 0)
            {
                byte length = (byte) ((amountLeftToSend < UART.MAXIMUM_FRAME_LENGTH ? amountLeftToSend : UART.MAXIMUM_FRAME_LENGTH) & 0xff);
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

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        private Message() { }

        /// <summary>
        ///     Construct a new message for the ESP32.
        /// </summary>
        /// <param name="device">Device / Interface the message is intended for.</param>
        /// <param name="function">Function that the device / interface should perform.</param>
        /// <param name="messageID">Unique ID of this message.</param>
        /// <param name="payload">Payload for this message.</param>
        public Message(byte device, UInt32 function, UInt32 messageID, byte[] payload)
        {
            if (payload?.Length > MAXIMUM_MESSAGE_LENGTH)
            {
                throw new ArgumentOutOfRangeException(nameof(payload), $"Payload length should be less than or equal to {MAXIMUM_MESSAGE_LENGTH} bytes");
            }

            Device = device;
            Function = function;
            MessageID = messageID;
            Payload = payload;
        }

        #endregion Constructors
    }
}
