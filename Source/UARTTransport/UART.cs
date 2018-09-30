using System;
using System.Diagnostics;
using System.Dynamic;
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

        /// <summary>
        ///     Communication states for the UART.
        /// </summary>
        private enum CommunicationStates
        {
            StartOfFrame = 0,
            FrameType,
            MessageID,
            SequenceNumber,
            Length,
            Payload,
            CRC,
            EndOfFrame
        };

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
        ///     Location of the frame type in the message FramePayload.
        /// </summary>
        private const int FRAME_TYPE = 0x00;

        /// <summary>
        ///     Seed for the CRC32 algorithm.
        /// </summary>
        private const UInt32 CRC32_SEED = 0xffffffff;

        /// <summary>
        ///     Location of the message ID in the message FramePayload.
        /// </summary>
        private const int MESSAGE_ID = FRAME_TYPE + 1;

        /// <summary>
        ///     Location of the sequence number in the message FramePayload.
        /// </summary>
        private const int SEQUENCE_ID = MESSAGE_ID + 4;

        /// <summary>
        ///     Location of the frame length in the message FramePayload.
        /// </summary>
        private const int LENGTH = SEQUENCE_ID + 1;

        /// <summary>
        ///     Size of the header.
        /// </summary>
        private const int HEADER_LENGTH = LENGTH + 1;

        /// <summary>
        ///     Maximum number of bytes that can be read from the serial port in one chunk.
        /// </summary>
        private const int RX_BUFFER_SIZE = 1024;

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

        /// <summary>
        ///     Last frame of data sent over the serial port.
        /// </summary>
        /// <remarks>
        ///The last frame is kept until and ACK or reset is received in case we need to resend the frame.
        /// </remarks>
        private byte[] _lastFrameSent = null;

        /// <summary>
        ///     Buffer to hold the data being read from the serial port.
        /// </summary>
        private readonly byte[] _buffer = new byte[RX_BUFFER_SIZE];

        #endregion Private member variables

        #region Properties

        /// <summary>
        ///     Number of consecutive occurrences of the start of frame character has been detected
        /// </summary>
        private int StartOfFrameByteCount { get; set; } = 0;

        /// <summary>
        ///     Type of frame being received.
        /// </summary>
        private byte CurrentFrameType { get; set; }

        /// <summary>
        ///     Number of bytes in the current data frame being received.
        /// </summary>
        private byte FramePayloadLength = 0;

        /// <summary>
        ///     Payload of the frame currently being received.
        /// </summary>
        private byte[] FramePayload = null;

        /// <summary>
        ///     Location of the next payload byte in the buffer.
        /// </summary>
        private byte PayloadIndex = 0;

        /// <summary>
        ///     Value of the checksum as read from the incoming data frame.
        /// </summary>
        private UInt32 OriginalChecksum { get; set; } = 0;

        /// <summary>
        ///     Value of the calculated checksum from the progressive checksum calculation.
        /// </summary>
        private UInt32 CalculatedChecksum { get; set;  } = 0;

        private int byteNumber = 0;

        /// <summary>
        ///     Message ID of the data packets being received.
        /// </summary>
        private UInt32 MessageID { get; set; } = 0;

        /// <summary>
        ///     Sequence number of this frame in the total number of frames being sent.
        /// </summary>
        private byte FrameSequenceNumber { get; set; } = 0;

        /// <summary>
        ///     Holds the current state of the communication state machine.
        /// </summary>
        private CommunicationStates CurrentCommunicationState { get; set; } = CommunicationStates.StartOfFrame;

        #endregion Properties

        #region Events

        /// <summary>
        ///     Process the data as it arrives at the serial port.
        /// </summary>
        /// <param name="sender">Object sending the serial data.</param>
        /// <param name="e">Serial data.</param>
        private void _port_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            if (e.EventType == SerialData.Chars)
            {
                int amount = ((SerialPort) sender).Read(_buffer, 0, RX_BUFFER_SIZE);
                if (amount > 0)
                {
                    for (int index = 0; index < amount; index++)
                    {
                        byte data = _buffer[index];

                        if (StartOfFrameByteCount == 2)
                        {
                            StartOfFrameByteCount = 0;
                            switch (data)
                            {
                                case START_OF_FRAME_CHARACTER:
                                    CurrentCommunicationState = CommunicationStates.FrameType;
                                    break;
                                case PADDING_CHARACTER:
                                    break;
                                default:
                                    CurrentCommunicationState = CommunicationStates.StartOfFrame;
                                    break;
                            }
                        }
                        else
                        {
                            if (data == START_OF_FRAME_CHARACTER)
                            {
                                StartOfFrameByteCount++;
                            }
                            else
                            {
                                StartOfFrameByteCount = 0;
                            }
                            switch (CurrentCommunicationState)
                            {
                                case CommunicationStates.StartOfFrame:
                                    //
                                    //  We don't need to do anything here as Start of Frame
                                    //  is dealt with above.
                                    //
                                    break;
                                case CommunicationStates.FrameType:
                                    CurrentFrameType = data;
                                    CurrentCommunicationState = CommunicationStates.MessageID;
                                    CalculatedChecksum = CRC32_SEED;
                                    CalculatedChecksum = ProgressiveCRC32(CalculatedChecksum, data);
                                    byteNumber = 0;
                                    break;
                                case CommunicationStates.MessageID:
                                    MessageID |= (UInt32) (data << (byteNumber * 8));
                                    byteNumber++;
                                    if (byteNumber == 4)
                                    {
                                        CurrentCommunicationState = CommunicationStates.SequenceNumber;
                                    }
                                    CalculatedChecksum = ProgressiveCRC32(CalculatedChecksum, data);
                                    break;
                                case CommunicationStates.SequenceNumber:
                                    FrameSequenceNumber = data;
                                    CurrentCommunicationState = CommunicationStates.Length;
                                    CalculatedChecksum = ProgressiveCRC32(CalculatedChecksum, data);
                                    break;
                                case CommunicationStates.Length:
                                    FramePayloadLength = data;
                                    CalculatedChecksum = ProgressiveCRC32(CalculatedChecksum, data);
                                    if (FramePayloadLength > 0)
                                    {
                                        FramePayload = new byte[FramePayloadLength];
                                        PayloadIndex = 0;
                                        CurrentCommunicationState = CommunicationStates.Payload;
                                    }
                                    else
                                    {
                                        CurrentCommunicationState = CommunicationStates.CRC;
                                        byteNumber = 0;
                                    }
                                    break;
                                case CommunicationStates.Payload:
                                    FramePayload[PayloadIndex] = data;
                                    PayloadIndex++;
                                    CalculatedChecksum = ProgressiveCRC32(CalculatedChecksum, data);
                                    if (PayloadIndex == FramePayloadLength)
                                    {
                                        CurrentCommunicationState = CommunicationStates.CRC;
                                        byteNumber = 0;
                                        OriginalChecksum = 0;
                                    }
                                    break;
                                case CommunicationStates.CRC:
                                    OriginalChecksum |= (UInt32) (data << (byteNumber * 8));
                                    byteNumber++;
                                    if (byteNumber == 4)
                                    {
                                        CurrentCommunicationState = CommunicationStates.EndOfFrame;
                                    }
                                    break;
                                case CommunicationStates.EndOfFrame:
                                    if ((data == END_OF_FRAME_CHARACTER) && (CalculatedChecksum == OriginalChecksum))
                                    {
                                        //ValidFrame(frameType, FramePayload, FramePayloadLength);
                                    }
                                    else
                                    {
                                        //InvalidFrame(frameType);
                                        CurrentCommunicationState = CommunicationStates.StartOfFrame;
                                    }
                                    if (FramePayload != null)
                                    {
                                        //vPortFree(FramePayload);
                                        FramePayload = null;
                                    }
                                    CurrentCommunicationState = CommunicationStates.StartOfFrame;
                                    break;
                                default:
                                    CurrentCommunicationState = CommunicationStates.StartOfFrame;
                                    break;
                            }
                        }
                    }
                }
            }
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
            _buffer = new byte[RX_BUFFER_SIZE];
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
            UInt32 crc = CRC32_SEED;
            ;

            for (int index = start; index < length; index++)
            {
                for (byte innerLoop = 0; innerLoop < 8; innerLoop++)
                {
                    crc = ProgressiveCRC32(crc, data[innerLoop]);
                }
            }

            return (crc);
        }

        /// <summary>
        ///     Progressively calculate the CRC32 value.
        /// </summary>
        /// <remarks>
        ///     This allows method allows the CRC32 value to be built up as the data bytes
        ///     from the serial port are received.  This means that all of the data frame
        ///     does not need to be present in order to calculate this value.
        /// </remarks>
        /// <param name="currentChecksum">Current value for the OriginalChecksum.</param>
        /// <param name="data">Data byte to be processed.</param>
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
        ///     Calculate the size of the frame .
        /// </summary>
        /// <remarks>
        ///     The frame is composed of the header, the data to be transmitted and the
        ///     end of frame marker.
        ///
        ///     The data part of the data frame has to take into account of the start of
        ///     frame character.  A number of start of frame characters in the data could
        ///     be interpreted as a new FramePayload.  A padding character is used to prevent
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
        /// <param name="sequenceNumber">Sequence number for this frame for the specified MessageID</param>
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
                byte length = (byte) ((amountLeftToSend < Frame.MAX_PAYLOAD_SIZE ? amountLeftToSend : Frame.MAX_PAYLOAD_SIZE) & 0xff);                
                SendFrame(frameType, messageID, sequenceNumber, message, start, length);
                start += length;
                amountLeftToSend -= length;
                sequenceNumber++;
            }
        }
        #endregion Methods
    }
}
