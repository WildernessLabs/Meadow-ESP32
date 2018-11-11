using System;
using System.Diagnostics;
using System.Dynamic;
using System.IO.Ports;
using System.Timers;

namespace UARTTransport
{
    public class UART
    {
        #region Enums

        /// <summary>
        ///     Frames types available to the application.
        /// </summary>
        public enum FrameType : byte
        {
            /// <summary>
            ///     Application header frame.
            /// </summary>
            ApplicationHeader = PKT_DATA + DATA_HEADER,

            /// <summary>
            ///     Application data frame.
            /// </summary>
            ApplicationData = PKT_DATA + DATA_PAYLOAD,

            /// <summary>
            ///     Transport packet, acknowledgement.
            /// </summary>
            ACK = PKT_TRANSPORT + TRANSPORT_ACK,

            /// <summary>
            ///     Transport packet, negative acknowledgement.
            /// </summary>
            NAK = PKT_TRANSPORT + TRANSPORT_NAK,

            /// <summary>
            ///     Transport packet, reset system.
            /// </summary>
            Reset = PKT_TRANSPORT + TRANSPORT_RESET
        };

        /// <summary>
        ///     Communication states for the UART.
        /// </summary>
        private enum CommunicationStates
        {
            StartOfFrame = 0,
            FrameType,
            StatusCode,
            MessageID,
            SequenceNumber,
            Length,
            Payload,
            CRC,
            EndOfFrame
        };

        /// <summary>
        ///     Errors that could be reported by the UART.
        /// </summary>
        /// <remarks>
        ///     The error codes and values here must be kept in step
        ///     with the error codes in the ESP system.
        /// </remarks>
        public enum ErrorCodes
        {
            /// <summary>
            ///     No error, frame has been processed OK.
            /// </summary>
            OK = 0,

            /// <summary>
            ///     CRC check has failed.
            /// </summary>
            CRCError = 1,

            /// <summary>
            ///     UART as requested that the message is restarted from
            ///     from the start.
            /// </summary>
            Restart = 2, 

            /// <summary>
            ///     General failure.
            /// </summary>
            Failure = 3,

            /// <summary>
            ///     Interface (device) not recognised.
            /// </summary>
            InvalidInterface = 4,

            /// <summary>
            ///     Error queuing the request.
            /// </summary>
            QueueError = 5,

            /// <summary>
            ///     Packet / frame is invalid.
            /// </summary>
            InvalidPacket = 6,

            /// <summary>
            ///     Header for the message is invalid.
            /// </summary>
            InvalidHeader = 7,

            /// <summary>
            ///     Unexpected frame.
            /// </summary>
            UnexpectedData = 8,

            /// <summary>
            ///     End of frame marker is missing or invalid.
            /// </summary>
            MissingEndOfFrameMarker = 9
        }

        #endregion Enums

        #region Constants

        /// <summary>
        ///     Bit mask for an application message.
        /// </summary>
        public const byte PKT_DATA = 0x80;

        /// <summary>
        ///     Bit mask for transport messages.
        /// </summary>
        public const byte PKT_TRANSPORT = 0x40;

        /// <summary>
        ///     Bit mask for the ACK message type.
        /// </summary>
        public const byte TRANSPORT_ACK = 0x01;

        /// <summary>
        ///     Bit mask for the NAK message type.
        /// </summary>
        public const byte TRANSPORT_NAK = 0x02;

        /// <summary>
        ///     Bit mask for the reset request message type.
        /// </summary>
        public const byte TRANSPORT_RESET = 0x04;

        /// <summary>
        ///     Bit mask for the application data message type.
        /// </summary>
        public const byte DATA_HEADER = 0x01;

        /// <summary>
        ///     Bit mask for the application data payload type.
        /// </summary>
        public const byte DATA_PAYLOAD = 0x02;

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
        ///     Location of the frame type in the message frame.
        /// </summary>
        private const int FRAME_TYPE = 0x00;

        /// <summary>
        ///     Location of the message ID in the message frame.
        /// </summary>
        private const int MESSAGE_ID = FRAME_TYPE + 1;

        /// <summary>
        ///     Location of the sequence number in the message frame.
        /// </summary>
        private const int SEQUENCE_ID = MESSAGE_ID + 4;

        /// <summary>
        ///     Location of the frame length in the message frame.
        /// </summary>
        private const int FRAME_PAYLOAD_LENGTH = SEQUENCE_ID + 1;

        /// <summary>
        ///     Size of the header.
        /// </summary>
        private const int FRAME_HEADER_LENGTH = FRAME_PAYLOAD_LENGTH + 1;

        /// <summary>
        ///     Maximum number of bytes in the frame payload.
        /// </summary>
        public const int MAXIMUM_FRAME_LENGTH = 255;

        /// <summary>
        ///     Seed for the CRC32 algorithm.
        /// </summary>
        private const UInt32 CRC32_SEED = 0xffffffff;

        /// <summary>
        ///     Maximum number of bytes that can be read from the serial port in one chunk.
        /// </summary>
        private const int RX_BUFFER_SIZE = 1024;

        /// <summary>
        ///     Maximum time (in milliseconds) between the end of transmission to a ACK / NAK being received.
        /// </summary>
        public const ushort TIMEOUT_MS = 500;

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
        ///     The last frame is kept until and ACK or reset is received in case we need to resend the frame.
        /// </remarks>
        private byte[] _lastFrameSent = null;

        /// <summary>
        ///     Buffer to hold the data being read from the serial port.
        /// </summary>
        private readonly byte[] _buffer = new byte[RX_BUFFER_SIZE];

        /// <summary>
        ///     Object used to lock the _time out Timer object.
        /// </summary>
        private object _timeoutTimerLock = new Object();

        /// <summary>
        ///     Timeout timer is used to work out if we should raise an error event
        ///     as there has been too long between sending the frame and receiving
        ///     an ACK / NAK.
        /// </summary>
        private System.Timers.Timer _timeoutTimer = null;

        /// <summary>
        ///     Retry counter used to work out if the system should resend the frame
        ///     or if it should generate an error.
        /// </summary>
        private uint _currentRetryCount = 0;

        /// <summary>
        ///     This variable holds the byte number when putting together multi-byte
        ///     values (i.e. 4 bytes in a UInt32 object).  this allows the original
        ///     object to be rehydrated from the UART byte stream.
        /// </summary>
        private int _byteNumber = 0;

        /// <summary>
        ///     Message ID of the data packets being received.
        /// </summary>
        private UInt32 _messageID = 0;

        /// <summary>
        ///     Sequence number of this frame in the total number of frames being sent.
        /// </summary>
        private byte _frameSequenceNumber = 0;

        /// <summary>
        ///     Holds the current state of the communication state machine.
        /// </summary>
        private CommunicationStates _communicationState = CommunicationStates.StartOfFrame;

        /// <summary>
        ///     Type of frame being received.
        /// </summary>
        private byte _currentFrameType = 0;

        /// <summary>
        ///     Number of consecutive occurrences of the start of frame character has been detected
        /// </summary>
        private int _startOfFrameByteCount = 0;

        /// <summary>
        ///     Value of the checksum as read from the incoming data frame.
        /// </summary>
        private UInt32 _originalChecksum = 0;

        /// <summary>
        ///     Value of the calculated checksum from the progressive checksum calculation.
        /// </summary>
        private UInt32 _calculatedChecksum = 0;

        /// <summary>
        ///     Number of bytes in the current data frame being received.
        /// </summary>
        private byte _framePayloadLength = 0;

        /// <summary>
        ///     Payload of the frame currently being received.
        /// </summary>
        private byte[] _framePayload = null;

        /// <summary>
        ///     Location of the next payload byte in the buffer.
        /// </summary>
        private byte _framePayloadIndex = 0;

        /// <summary>
        ///     Packet code returned by the transport packets.
        /// </summary>
        private UInt32 _statusCode = 0;

        #endregion Private member variables

        #region Properties

        /// <summary>
        ///     Maximum retry count for serial communication.
        /// </summary>
        private uint MaximumRetryCount { get; set; } = 3;

        #endregion Properties

        #region Delegates

        /// <summary>
        ///     Packet event.
        /// </summary>
        public event UARTResponseEventHandler ResponseReceived = delegate { };

        /// <summary>
        ///     Data received event.
        /// </summary>
        public event UARTDataReceivedEventHandler DataReceived = delegate { };

        #endregion Delegates

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
                Debug.Print($"\nData received ({amount}) bytes:");
                DisplayFrame(_buffer, amount);
                
                if (amount > 0)
                {
                    for (int index = 0; index < amount; index++)
                    {
                        byte data = _buffer[index];

                        if (_startOfFrameByteCount == 2)
                        {
                            _startOfFrameByteCount = 0;
                            switch (data)
                            {
                                case START_OF_FRAME_CHARACTER:
                                    _communicationState = CommunicationStates.FrameType;
                                    break;
                                case PADDING_CHARACTER:
                                    break;
                                default:
                                    _communicationState = CommunicationStates.StartOfFrame;
                                    break;
                            }
                        }
                        else
                        {
                            if (data == START_OF_FRAME_CHARACTER)
                            {
                                _startOfFrameByteCount++;
                            }
                            else
                            {
                                _startOfFrameByteCount = 0;
                            }
                            switch (_communicationState)
                            {
                                case CommunicationStates.StartOfFrame:
                                    //
                                    //  We don't need to do anything here as Start of Frame
                                    //  is dealt with above.
                                    //
                                    break;
                                case CommunicationStates.FrameType:
                                    _currentFrameType = data;
                                    if ((_currentFrameType & PKT_TRANSPORT) > 0)
                                    {
                                        _communicationState = CommunicationStates.StatusCode;
                                    }
                                    else
                                    {
                                        _communicationState = CommunicationStates.MessageID;
                                    }
                                    _calculatedChecksum = CRC32_SEED;
                                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, data);
                                    _byteNumber = 0;
                                    break;
                                case CommunicationStates.StatusCode:
                                    _statusCode |= (UInt32) (data << (_byteNumber * 8));
                                    _byteNumber++;
                                    if (_byteNumber == 4)
                                    {
                                        _communicationState = CommunicationStates.CRC;
                                        _byteNumber = 0;
                                        _originalChecksum = 0;
                                    }
                                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, data);
                                    break;
                                case CommunicationStates.MessageID:
                                    _messageID |= (UInt32) (data << (_byteNumber * 8));
                                    _byteNumber++;
                                    if (_byteNumber == 4)
                                    {
                                        _communicationState = CommunicationStates.SequenceNumber;
                                    }
                                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, data);
                                    break;
                                case CommunicationStates.SequenceNumber:
                                    _frameSequenceNumber = data;
                                    _communicationState = CommunicationStates.Length;
                                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, data);
                                    break;
                                case CommunicationStates.Length:
                                    _framePayloadLength = data;
                                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, data);
                                    if (_framePayloadLength > 0)
                                    {
                                        _framePayload = new byte[_framePayloadLength];
                                        _framePayloadIndex = 0;
                                        _communicationState = CommunicationStates.Payload;
                                    }
                                    else
                                    {
                                        _communicationState = CommunicationStates.CRC;
                                        _byteNumber = 0;
                                        _originalChecksum = 0;
                                    }
                                    break;
                                case CommunicationStates.Payload:
                                    _framePayload[_framePayloadIndex] = data;
                                    _framePayloadIndex++;
                                    _calculatedChecksum = ProgressiveCRC32(_calculatedChecksum, data);
                                    if (_framePayloadIndex == _framePayloadLength)
                                    {
                                        _communicationState = CommunicationStates.CRC;
                                        _byteNumber = 0;
                                        _originalChecksum = 0;
                                    }
                                    break;
                                case CommunicationStates.CRC:
                                    _originalChecksum |= (UInt32) (data << (_byteNumber * 8));
                                    _byteNumber++;
                                    if (_byteNumber == 4)
                                    {
                                        _communicationState = CommunicationStates.EndOfFrame;
                                    }
                                    break;
                                case CommunicationStates.EndOfFrame:
                                    if ((data == END_OF_FRAME_CHARACTER) && (_calculatedChecksum == _originalChecksum))
                                    {
                                        if ((_currentFrameType & PKT_TRANSPORT) > 0)
                                        {
                                            ProcessTransportData(_currentFrameType);
                                        }
                                        else
                                        {
                                            ProcessApplicationDataFrame(_currentFrameType, _framePayload, _framePayloadLength);
                                        }
                                    }
                                    else
                                    {
                                        if (data != END_OF_FRAME_CHARACTER)
                                        {
                                            Console.WriteLine("Invalid frame");
                                        }
                                        else
                                        {
                                            ProcessTransportData(_currentFrameType);
                                        }
                                    }
                                    if (_framePayload != null)
                                    {
                                        //vPortFree(_framePayload);
                                        _framePayload = null;
                                    }
                                    _communicationState = CommunicationStates.StartOfFrame;
                                    break;
                                default:
                                    _communicationState = CommunicationStates.StartOfFrame;
                                    break;
                            }
                        }
                    }
                }
            }
        }

        /// <summary>
        ///     The system should receive a response with a maximum time period.
        /// </summary>
        /// <param name="sender">Object generating this event.</param>
        /// <param name="e">Event data for the timer Elapsed event.</param>
        private void _timeoutTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            lock (_timeoutTimerLock)
            {
                _timeoutTimer.Enabled = false;
            }
            _currentRetryCount++;
            Console.WriteLine($"    Timeout sending message {_currentRetryCount}");
            if (_currentRetryCount > MaximumRetryCount)
            {
                UARTResponseEventArgs timeout = new UARTResponseEventArgs(UARTResponseEventArgs.TransportPacketType.Timeout, 0);
                ResponseReceived(this, timeout);
            }
            else
            {
                Send(_lastFrameSent);
                lock (_timeoutTimerLock)
                {
                    _timeoutTimer.Enabled = true;
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
        /// <param name="serialPortName">Name of the serial port connected to the ESP32.</param>
        /// <param name="maximumRetries">Maximum number of retries should a timeout or NAK occur.</param>
        public UART(string serialPortName, uint maximumRetries = 3)
        {
            _port = new SerialPort(serialPortName, 57600, Parity.None, 8, StopBits.One);
            _port.DataReceived += _port_DataReceived;
            _port.Open();
            for (int index = 0; index < _startOfFrame.Length; index++)
            {
                _startOfFrame[index] = START_OF_FRAME_CHARACTER;
            }
            _buffer = new byte[RX_BUFFER_SIZE];
            MaximumRetryCount = maximumRetries;
            //
            //  Create a new timeout timer but do not start it.  The timer should
            //  be set for a manual restart.
            //
            _timeoutTimer = new Timer(TIMEOUT_MS);
            _timeoutTimer.Enabled = false;              // Don't start it yet.
            _timeoutTimer.AutoReset = false;            // Manual restart.
            _timeoutTimer.Elapsed += _timeoutTimer_Elapsed;
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

            for (int index = start; index < length; index++)
            {
                crc = ProgressiveCRC32(crc, data[index]);
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
        ///     Calculate the size of the frame.
        /// </summary>
        /// <remarks>
        ///     The frame is composed of the header, the data to be transmitted and the
        ///     end of frame marker.
        ///
        ///     The data part of the data frame has to take into account of the start of
        ///     frame character.  A number of start of frame characters in the data could
        ///     be interpreted as a new _framePayload.  A padding character is used to prevent
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
            byteCount += FRAME_HEADER_LENGTH;   //  Size of the header.
            byteCount += 4;                     //  CRC is a 32-bit CRC and takes 4 bytes.
            return(byteCount);
        }

        /// <summary>
        ///     Copy the data into the new frame taking into account the need to insert
        ///     padding characters if we hit a sequence of start of frame characters.
        /// </summary>
        private int EncodeData(byte[] message, byte[] frame, int start)
        {
            int currentByte = 0;
            int startOfFrameCharacterCount = 0;
            for (int index = 0; index < message.Length; index++)
            {
                frame[start + currentByte] = message[index];
                currentByte++;
                if (message[index] == START_OF_FRAME_CHARACTER)
                {
                    startOfFrameCharacterCount++;
                    if (startOfFrameCharacterCount == (NUMBER_OF_START_OF_FRAME_CHARACTERS - 1))
                    {
                        frame[start + currentByte] = PADDING_CHARACTER;
                        currentByte++;
                        startOfFrameCharacterCount = 0;
                    }
                }
                else
                {
                    startOfFrameCharacterCount = 0;
                }
            }
            return(currentByte);
        }

        /// <summary>
        ///     Generate a data frame from the message.
        /// </summary>
        /// <param name="frameType">Frame type (transport, application etc.)</param>
        /// <param name="messageID">Unique message ID for the message.</param>
        /// <param name="sequenceNumber">Sequence number for this frame for the specified _messageID</param>
        /// <param name="message">Array of bytes containing the message to be sent.</param>
        /// <returns>Frame with CRC ready for transmission over the UART.</returns>
        private byte[] GenerateApplicationFrame(FrameType frameType, UInt32 messageID, byte sequenceNumber, byte[] message)
        {
            byte[] frame = new byte[HeaderAndPayloadSize(message)];

            int currentByte = 0;
            //
            //  Now add the frame header information.
            //
            frame[currentByte++] = (byte) frameType;
            Encoders.EncodeUInt32(messageID, frame, currentByte);
            currentByte += 4;
            frame[currentByte++] = sequenceNumber;
            frame[currentByte++] = (byte) (message.Length & 0xff);
            currentByte += EncodeData(message, frame, currentByte);
            //
            //  Now add the CRC to the frame.
            //
            Encoders.EncodeUInt32(CRC32(frame, 0, frame.Length - 4), frame, currentByte);
            return(frame);
        }

        private byte[] GenerateTransportFrame(FrameType frameType, byte[] message, int frameLength)
        {
            byte[] frame = new byte[HeaderAndPayloadSize(message)];
            int currentByte = 0;
            
            currentByte += EncodeData(message, frame, 0);
            //
            //  The frame length calculated contains the header characters, CRC
            //  and the end of frame characters.  We need to take this into consideration 
            //  when the CRC is calculated as the CRC should only be calculated on
            //  the actual data plus any padding characters.
            //
            UInt32 crc = CRC32(frame, frame.Length - 4);
            Encoders.EncodeUInt32(crc, frame, currentByte);
            currentByte += 4;
            frame[currentByte] = END_OF_FRAME_CHARACTER;
            return (frame);
        }

        /// <summary>
        ///     Send a frame of data over the serial port.
        /// </summary>
        /// <param name="frame">Byte array containing the raw data to be sent.</param>
        private void Send(byte[] frame)
        {
            _port.Write(_startOfFrame, 0, _startOfFrame.Length);
            _port.Write(_lastFrameSent, 0, _lastFrameSent.Length);
            _port.Write(new[] { END_OF_FRAME_CHARACTER }, 0, 1);
        }

        /// <summary>
        ///     Send a frame of data on the serial port.
        /// </summary>
        /// <param name="frameType">Frame type.</param>
        /// <param name="messageID">ID of the message that this frame belongs to.</param>
        /// <param name="sequenceNumber">Sequence number of the frame.</param>
        /// <param name="message">Full message to send (not just the frame).</param>
        public void Send(FrameType frameType, UInt32 messageID, byte sequenceNumber, byte[] message)
        {
            if (message.Length > MAXIMUM_FRAME_LENGTH)
            {
                throw new ArgumentOutOfRangeException(nameof(message),
                    $"Message must be smaller than {MAXIMUM_FRAME_LENGTH} bytes.");
            }

            _lastFrameSent = GenerateApplicationFrame(frameType, messageID, sequenceNumber, message);
            _currentRetryCount = 0;
            DisplayFrame(_lastFrameSent);
            Send(_lastFrameSent);
            _timeoutTimer.Enabled = true;       // Start the timeout timer.
        }

        /// <summary>
        ///     An invalid frame has been sent, send a NAK to the STM32.
        /// </summary>
        /// <param name="frameType"></param>
        void InvalidFrame(byte frameType)
        {
        }

        /// <summary>
        ///     Send a positive (ACK) or negative (NAK) acknowledgement along with a response (error) code.
        /// </summary>
        /// <param name="ackNak">Type of acknowledgement (ACK ot NAK).</param>
        /// <param name="status">Status code to send in the ACK / NAK message.</param>
        void AcknowledgeFrame(byte ackNak, UInt32 status)
        {
        }

        /// <summary>
        ///     Process the transport request.
        /// </summary>
        private void ProcessTransportData(byte currentFrameType)
        {
            switch (currentFrameType & 0x2f)
            {
                case TRANSPORT_ACK:
                    lock (_timeoutTimerLock)
                    {
                        _timeoutTimer.Enabled = false;
                    }
                    UARTResponseEventArgs ack = new UARTResponseEventArgs(UARTResponseEventArgs.TransportPacketType.ACK, _statusCode);
                    ResponseReceived(this, ack);
                    break;
                case TRANSPORT_NAK:
                    lock (_timeoutTimerLock)
                    {
                        _timeoutTimer.Enabled = false;
                    }
                    _currentRetryCount++;
                    Console.WriteLine($"    NAK, retrying {_currentRetryCount}");
                    if (_currentRetryCount > MaximumRetryCount)
                    {
                        UARTResponseEventArgs nak = new UARTResponseEventArgs(UARTResponseEventArgs.TransportPacketType.NAK, _statusCode);
                        ResponseReceived(this, nak);
                    }
                    else
                    {
                        Send(_lastFrameSent);
                        lock (_timeoutTimerLock)
                        {
                            _timeoutTimer.Enabled = true;
                        }
                    }
                    break;
                case TRANSPORT_RESET:
                    //
                    //  Reset the interface back to power on.
                    //
                    //ResetIncomingMessage(true);
                    break;
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="packet"></param>
        /// <param name="packetLength"></param>
        private void ProcessHeader(byte[] packet, byte packetLength)
        {
            //if (packetLength != APP_RESPONSE_HEADER_LENGTH)
            //{
            //    ESP_LOGI(UART_RX_TASK_NAME, "Invalid header legth: %d", packetLength);
            //    AcknowledgeFrame(MSG_NAK, ERR_INVALID_HEADER);
            //}
            //else
            //{
            //    ResetIncomingMessage(true);
            //    messageHeader.interface = packet[0];
            //    messageHeader.function = ExtractUInt32(packet + APP_REQUEST_FUNCTION);
            //    messageHeader.messageID = ExtractUInt32(packet + APP_REQUEST_MESSAGE_ID);
            //    bytesRemaining = ExtractUInt16(packet + APP_REQUEST_PAYLOAD_LENGTH);
            //    messageHeader.payloadLength = bytesRemaining;
            //    if (messageHeader.payloadLength > 0)
            //    {
            //        messageHeader.payload = pvPortMalloc(messageHeader.payloadLength);
            //    }
            //    else
            //    {
            //        messageHeader.payload = NULL;
            //        //
            //        //  Messages with no payload should be queued immediately.
            //        //
            //        QueueMessage();
            //    }
            //    nextPayloadLocation = messageHeader.payload;
            //    AcknowledgeFrame(MSG_ACK, ERR_OK);
            //}
        }

        private void ProcessData(byte[] packet, byte packetLength)
        {

        }

        /// <summary>
        ///     A valid frame has been received, work out what to do with it.
        /// </summary>
        /// <param name="frameType">Type of frame that has been received (transport, application etc.)</param>
        /// <param name="packet">Payload to be processed.</param>
        /// <param name="packetLength">Amount of data in the payload.</param>
        private void ProcessApplicationDataFrame(byte frameType, byte[] packet, byte packetLength)
        {
            //
            //  Application data packets are either header or data packets.
            //
            if ((frameType & DATA_HEADER) > 0)
            {
                ProcessHeader(packet, packetLength);
            }
            else
            {
                ProcessData(packet, packetLength);
            }
        }

        /// <summary>
        ///     Dump the array of bytes to the debug output in hexadecimal.
        /// </summary>
        private void DisplayFrame(byte[] frame, int amount = -1)
        {
            var line = string.Empty;
            Debug.Print("\n\n       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
            if (amount < 0)
            {
                amount = frame.Length;
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
                line += $"{frame[index]:X2} ";
            }
            if (line != string.Empty)
            {
                Debug.Print(line);
            }

            Debug.Print("\n");
        }

        #endregion Methods
    }
}
