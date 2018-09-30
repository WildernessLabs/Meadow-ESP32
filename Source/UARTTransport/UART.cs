using System;
using System.IO.Ports;

namespace UARTTransport
{
    public class UART
    {
        #region Enums

        /// <summary>
        ///     Frames types available to the application.
        /// </summary>
        public enum FrameType :  byte { Header = 0x81, Data = 0x82 };

        #endregion Enums

        #region Constants

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
        private const int MAX_FRAME_SIZE = 255;

        #endregion Constants

        #region Private member variables

        /// <summary>
        ///     Serial port to which the ESP32 is connected.
        /// </summary>
        private readonly SerialPort _port;

        /// <summary>
        ///     Array holding the start of frame characters.
        /// </summary>
        private readonly byte[] _startOfFrame = new byte[NUMBER_OF_START_OF_FRAME_CHARACTERS];

        #endregion Private member variables

        #region Events

        /// <summary>
        ///     Process the data as it arrives at the serial port.
        /// </summary>
        /// <param name="sender">Object sending the serial data.</param>
        /// <param name="e">Serial data.</param>
        private void _port_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            throw new NotImplementedException();
        }

        #endregion Events

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
            _port.DataReceived += _port_DataReceived;
            _port.Open();
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
        /// </remarks>
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
            byteCount += HEADER_LENGTH;   //  Size of the header.
            byteCount += 4;             //  CRC is a 32-bit CRC and takes 4 bytes.
            return(byteCount);
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
        private byte[] GenerateFrame(FrameType frameType, UInt32 messageID, byte sequenceNumber, byte[] message, UInt16 offset, byte length)
        {
            byte[] frame = new byte[HeaderAndPayloadSize(message) + 4];

            int currentByte = 0;
            //
            //  Now add the frame header information.
            //
            frame[currentByte++] = (byte) frameType;
            Encoders.EncodeUInt32(messageID, frame, currentByte);
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
            Encoders.EncodeUInt32(CRC32(frame, 0, frame.Length - 4), frame, currentByte);
            return(frame);
        }

        /// <summary>
        ///     Send a frame of data on the serial port.
        /// </summary>
        /// <remarks>
        ///     TODO: ACK / NAK / ABORT / RESET - deal with the different return types.
        /// </remarks>
        /// <param name="frameType">Frame type.</param>
        /// <param name="messageID">ID of the message that this frame belongs to.</param>
        /// <param name="sequenceNumber">Sequence number of the frame.</param>
        /// <param name="message">Full message to send (not just the frame).</param>
        /// <param name="offset">Offset into the message of the first byte of this frame.</param>
        /// <param name="length">Number of payload bytes in the frame.</param>
        private void SendFrame(FrameType frameType, UInt32 messageID, byte sequenceNumber, byte[] message, UInt16 offset, byte length)
        {
            byte[] frame = GenerateFrame(frameType, messageID, sequenceNumber, message, offset, length);
            _port.Write(_startOfFrame, 0, _startOfFrame.Length);
            _port.Write(frame, offset, frame.Length);
            _port.Write(new byte[] { END_OF_FRAME_CHARACTER }, 0, 1);
            //
            //  TODO: Work out what to do about ACK etc.
            //
        }

        /// <summary>
        ///     Send the specified message on the serial line.
        /// </summary>
        /// <param name="frameType">Frame type to use to send the messages.</param>
        /// <param name="messageID">Message ID for the frames.</param>
        /// <param name="message">Byte array of data to be transmitted on the UART.</param>
        public void SendMessage(FrameType frameType, UInt32 messageID, byte[] message)
        {
            UInt16 start = 0;
            byte sequenceNumber = 0;
            int amountLeftToSend = message.Length;

            while (amountLeftToSend > 0)
            {
                byte length = (byte) ((amountLeftToSend < MAX_FRAME_SIZE ? amountLeftToSend : MAX_FRAME_SIZE) & 0xff);                
                SendFrame(frameType, messageID, sequenceNumber, message, start, length);
                start += length;
                amountLeftToSend -= length;
                sequenceNumber++;
            }
        }
        #endregion Methods
    }
}
