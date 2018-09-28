using System;
using System.IO.Ports;

namespace UARTTransport
{
    public class UART
    {
        /// <summary>
        ///     Number of START_OF_FRAME_CHARACTERS to use to indicate the
        ///     start of a data frame.
        /// </summary>
        public const int NUMBER_OF_START_OF_FRAME_CHARACTERS = 3;

        /// <summary>
        ///     Byte value to use to indicate the start of a frame.
        /// </summary>
        public const byte START_OF_FRAME_CHARACTER = 0xaa;

        /// <summary>
        ///     Byte value to use as the end of frame marker.
        /// </summary>
        public const byte END_OF_FRAME_CHARACTER = 0x55;

        /// <summary>
        ///     Byte value to use as a padding character.
        /// </summary>
        private const byte PADDING_CHARACTER = 0x52;

        /// <summary>
        ///     Location of the frame type in the message packet.
        /// </summary>
        private const int FRAME_TYPE = 0x00;

        /// <summary>
        ///     Location of the message ID in the message packet.
        /// </summary>
        private const int MESSAGE_ID = FRAME_TYPE + 1;

        /// <summary>
        ///     Location of the sequence number in the message packet.
        /// </summary>
        private const int SEQUENCE_ID = MESSAGE_ID + 4;

        /// <summary>
        ///     Location of the frame length in the message packet.
        /// </summary>
        private const int LENGTH = SEQUENCE_ID + 1;

        /// <summary>
        ///     Size of the header.
        /// </summary>
        private const int HEADER_LENGTH = LENGTH + 1;

        /// <summary>
        ///     Maximum number of bytes (un-padded that can be added to a payload)
        /// </summary>
        private const int MAX_UNPADDED_PAYLOAD_SIZE = 255;

        #region Private member variables

        /// <summary>
        ///     Serial port to which the ESP32 is connected.
        /// </summary>
        private SerialPort _port;

        /// <summary>
        ///     Array holding the start of frame characters.
        /// </summary>
        private byte[] _startOfFrame = new byte[NUMBER_OF_START_OF_FRAME_CHARACTERS];

        #endregion Private member variables

        #region Constructors

        /// <summary>
        ///     Default constructor (private to prevent it being used).
        /// </summary>
        private UART()
        {            
        }

        /// <summary>
        ///     Create a new UART object on the specified serial port.
        /// </summary>
        public UART(string serialPortName)
        {
             _port = new SerialPort(serialPortName.ToUpper(), 115200, Parity.None, 8, StopBits.None);
             for (int index = 0; index < _startOfFrame.Length; index++)
             {
                 _startOfFrame[index] = START_OF_FRAME_CHARACTER;
             }
        }

        #endregion Constructors

        #region Methods

        /// <summary>
        ///     Calculate the 32-bit CRC for the array of bytes.
        /// </summary>
        /// <param name="data">Buffer of data.</param>
        /// <returns>32-bit CRC for the array of bytes.</returns>
        private static UInt32 CRC32(byte[] buffer)
        {
            return(CRC32(buffer, 0, buffer.Length));
        }

        /// <summary>
        ///     Calculate the 32-bit CRC for the array of bytes.
        /// </summary>
        /// <param name="data">Buffer of data.</param>
        /// <param name="start">Offset into the buffer at which the CRC calculation will start</param>
        /// <param name="length">Amount of data to calculate the CRC</param>
        /// <returns>32-bit CRC for the array of bytes.</returns>
        private static UInt32 CRC32(byte[] data, int start, int length)
        {
            UInt32 crc = 0xffffffff;

            for (int index = start; index < length; index++)
            {
                crc ^= data[index];
                for (byte innerLoop = 0; innerLoop < 8; innerLoop++)
                {
                    UInt32 mask = (UInt32) (-(crc & 0x01));
                    crc = (crc >> 1) ^ (0xedb88320 & mask);
                }
            }

            return (crc);
        }

        /// <summary>
        ///     Calculate the size of the frame .
        /// </summary>
        /// <remarks>
        ///     The frame is composed of the header, the data to be transmitted and the
        ///     end of frame marker.
        ///
        ///     The data part of the data frame has to take into account of the start of
        ///     frame character.  A number of start of frame characters in the data could
        ///     be interpreted as a new packet.  A padding character is used to prevent
        ///     this.
        /// <remarks>
        /// <param name="buffer">Buffer containing the data to be checked.</param>
        private int HeaderAndPayloadSize(byte[] buffer)
        {
            int startOfFrameCharacterCount = 0;
            int byteCount = 0;
            
            for (int index = 0; index < buffer.Length; index++)
            {
                if (buffer[index] == START_OF_FRAME_CHARACTER)
                {
                    startOfFrameCharacterCount++;
                    if (startOfFrameCharacterCount == (NUMBER_OF_START_OF_FRAME_CHARACTERS - 1))
                    {
                        byteCount++;    // An additional byte is needed for the padding character.
                        startOfFrameCharacterCount = 0;
                    }
                }
                else
                {
                    startOfFrameCharacterCount = 0;
                }
                byteCount++;
            }
            byteCount += HEADER_SIZE;   //  Size of the header.
            byteCount += 4;             //  CRC is a 32-bit CRC and takes 4 bytes.
            return(byteCount);
        }

        /// <summary>
        ///     Take two bytes from the buffer and encode them as a 16-bit integer.
        ///     Note that the data should be encoded as LSB first.
        /// </summary>
        /// <param name="buffer">Buffer holding the value.</param>
        /// <param name="offset">Offset into the buffer for the LSB.</param>
        public UInt16 ExtractUInt16(byte[] buffer, int offset)
        {
            UInt16 result;

            result = buffer[offset];
            result |= (buffer[offset + 1] << 8);
            return (result);
        }

        /// <summary>
        ///     Encode a 16-bit integer as two bytes in the buffer.  The data is
        ///     encoded LSB first.
        /// </summary>
        /// <param name="value">Value to encode.</param>
        /// <param name="buffer">Buffer to store the value.</param>
        /// <param name="offset">Offset into the buffer for the first byte to store.</param>
        void EncodeUInt16(UInt16 value, byte[] buffer, int offset)
        {
            buffer[offset] = (value & 0xff);
            buffer[offset + 1] = ((value >> 8) & 0xff);
        }

        /// <summary>
        ///     Take four bytes from the buffer and encode them as a 32-bit integer.
        ///     Note that the data should be encoded as LSB first.
        /// </summary>
        /// <param name="buffer">Buffer holding the 32-bit value to decode.</param>
        /// <param name="offset">Offset of the 32-bit in the buffer.</param>
        UInt32 ExtractUInt32(byte[] buffer, int offset)
        {
            UInt32 result;

            result = buffer[offset];
            result |= (buffer[offset + 1] << 8);
            result |= (buffer[offset + 2] << 16);
            result |= (buffer[offset + 3] << 24);
            return (result);
        }

        /// <summary>
        ///     Encode a 32-bit integer as four bytes in the buffer.  The data is
        ///     encoded LSB first.
        /// </summary>
        /// <param name="value">Value to encode.</param>
        /// <param name="buffer">Buffer to store the value.</param>
        /// <param name="offset">Offset into the buffer for the first byte to store.</param>
        void EncodeUInt32(UInt32 value, byte[] buffer, int offset)
        {
            buffer[offset] = (value & 0xff);
            buffer[offset + 1] = ((value >> 8) & 0xff);
            buffer[offset + 2] = ((value >> 16) & 0xff);
            buffer[offset + 3] = ((value >> 24) & 0xff);
        }

        /// <summary>
        ///     Generate a data frame from the message.
        /// </summary>
        /// <param name="frameType">Frame type (transport, application etc.)</param>
        /// <param name="messageID">Unique message ID for the message.</param>
        /// <param name="sequenceNumber">Sequence number for this frame for the specified messageID</param>
        /// <param name="message">Array of bytes containing the message to be sent.</param>
        /// <param name="offset">Index (in the message) of the first byte of the frame.</param>
        /// <param name="length">Number of bytes in the frame.</param>
        /// <returns></returns>
        private byte[] GenerateFrame(byte frameType, Uint32 messageID, byte sequenceNumber, byte[] message, byte offset, byte length)
        {
            byte[] frame = new byte[HeaderAndPayloadSize(message) + 4];

            int currentByte = 0;
            //
            //  Now add the frame header information.
            //
            frame[currentByte++] = frameType;
            EncodeUInt32(messageID, frame, currentByte);
            currentByte += 4;
            frame[currentByte++] = sequenceNumber;
            frame[currentByte++] = length;
            //
            //  Copy the data into the new frame taking into account the need to insert
            //  padding characters if we hit a sequence of start of frame characters.
            //
            int startOfFrameCharacterCount = 0;
            for (int index = offset; index < (offset + length); index++)
            {
                frame[currentByte++] = message[index];
                if (message[index] == START_OF_FRAME_CHARACTER)
                {
                    startOfFrameCharacterCount++;
                    if (startOfFrameCharacterCount == (NUMBER_OF_START_OF_FRAME_CHARACTERS - 1))
                    {
                        frame[currentByte++] = PADDING_CHARACTER;
                        startOfFrameCharacterCount = 0;
                    }
                }
                else
                {
                    startOfFrameCharacterCount = 0;
                }
            }
            //
            //  Now add the CRC to the frame.
            //
            EncodeUInt32(CRC32(frame, 0, frame.Length - 4), frame, currentByte);
            return(frame);
        }

        /// <summary>
        ///     Send the specified message on the serial line.
        /// </summary>
        /// <param name="message">Byte array of data to be transmitted on the UART.</param>
        public void SendMessage(byte[] message)
        {
            int start = 0;
            byte sequenceNumber = 0;

            while (start < message.Length)
            {

            }
        }

        /// <summary>
        ///     Send the specified message on the serial line.
        /// </summary>
        /// <param name="header">Header for the message.</param>
        /// <param name="message">Message to be sent.</param>
        public void SendMessage(UInt32 messageID, byte[] header, byte[] message)
        {
            SendMessage(header);
            SendMessage(message);
        }

        #endregion Methods
    }
}
