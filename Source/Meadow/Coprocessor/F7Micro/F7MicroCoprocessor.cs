using System;
using System.Collections;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using Meadow;
using Meadow.Coprocessor.F7Micro.Exceptions;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Network;
using Meadow.Gateway.WiFi;
using Meadow.Coprocessor.F7Micro.Messages;

namespace Meadow.Coprocessor.F7Micro
{
    public partial class F7MicroCoprocessor : IPosixApi
    {
        #region Enums
        
        /// <summary>
        /// Possible debug outputs.
        /// </summary>
        [Flags]
        public enum DebugOptions : UInt32 { None = 0x00, Information = 0x01, Errors = 0x02, Full = 0xffffffff }
        
        #endregion

        #region Private member variables (fields)
        
        /// <summary>
        /// Message dispatcher for the coprocessor.
        /// </summary>
        private readonly MessageDispatcher _dispatcher;
        
        #endregion Private member variables (fields)
        
        #region Properties
        
        /// <summary>
        /// Determine how much debug output is generated.
        /// </summary>
        public DebugOptions DebugLevel{ get; set; }
        
        #endregion Properties
        
        #region Constructors
        
        /// <summary>
        /// Default constructor (private to prevent it being used).
        /// </summary>
        private F7MicroCoprocessor()
        {
        }
        
        /// <summary>
        /// Create a new coprocessor object and specify the message dispatcher to be used to send requests
        /// to the coprocessor.
        /// </summary>
        /// <param name="dispatcher">Message dispatcher to be used to send messages to the network chip.</param>
        public F7MicroCoprocessor(MessageDispatcher dispatcher)
        {
            _dispatcher = dispatcher;
        }
        
        #endregion Constructors

        #region Methods

        /// <summary>
        /// Get the antenna currently in use.
        /// </summary>
        /// <returns>AntennaTypes object indicating the currently selected antenna.</returns>
        public AntennaTypes GetAntenna()
        {
            //
            //  Prepare the request and wait object.
            //
            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (byte)WiFiFunction.GetAntenna,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = null,
                PayloadLength = 0
            };

            var response = _dispatcher.SendMessage(message);
            var antenna = Encoders.ExtractAntennaInfo(response.Payload, 0);
            return((AntennaTypes) antenna.Antenna);
        }

        /// <summary>
        /// Change the current antenna in use.
        /// </summary>
        /// <param name="antenna">New antenna to use.</param>
        public void SetAntenna(AntennaTypes antenna)
        {
            //
            //  Prepare the request and wait object.
            //
            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (byte)WiFiFunction.SetAntenna,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data
            };

            var antennaInfo = new AntennaInfo { Antenna = (byte)antenna };
            var encodedAntenna = Encoders.EncodeAntennaInfo(antennaInfo);
            message.Payload = encodedAntenna;
            message.PayloadLength = (UInt32) encodedAntenna.Length;
            
            _dispatcher.SendMessage(message);
        }

        /// <summary>
        /// Start the WiFi hardware and connect to the specified network.
        /// </summary>
        /// <param name="ssid">Name of the network to connect to.</param>
        /// <param name="password">Password for the network.</param>
        public IPAddress StartNetwork(string ssid, string password)
        {
            var startNetwork = new WiFiCredentials { NetworkName = ssid, Password = password };

            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.Start,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data
            };
            var encodedStartMessage = Encoders.EncodeWiFiCredentials(startNetwork);
            message.Payload = encodedStartMessage;
            message.PayloadLength = (uint)encodedStartMessage.Length;

            var response = _dispatcher.SendMessage(message);

            var address = new IPAddress(new Byte[] { response.Payload[0], response.Payload[1], response.Payload[2], response.Payload[3] });
            return (address);
        }

        /// <summary>
        /// Get the list of access points.
        /// </summary>
        /// <remarks>
        /// The network must be started before this method can be called.
        /// </remarks>
        /// <returns>List of access points (or null if no access points can be found).</returns>
        public List<WifiNetwork> GetAccessPoints()
        {
            var message = new Message
            {
                MessageType = (byte)MessageTypes.Data,
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (UInt32)WiFiFunction.GetAccessPoints,
                MessageID = 0x02,
                PayloadLength = 0
            };

            var response = _dispatcher.SendMessage(message);
            //
            //  Now get the list of access points and return the list to the application.
            //
            var accessPointList = Encoders.ExtractAccessPointList(response.Payload, 0);
            var accessPoints = new AccessPoint[accessPointList.NumberOfAccessPoints]; 
            var networks = new List<WifiNetwork>();
            if (accessPointList.NumberOfAccessPoints > 0)
            {
                int accessPointOffset = 0;
                for (int count = 0; count < accessPointList.NumberOfAccessPoints; count++)
                {
                    var accessPoint = Encoders.ExtractAccessPoint(accessPointList.AccessPoints, accessPointOffset);
                    accessPointOffset += Encoders.EncodedAccessPointBufferSize(accessPoint);
                    string bssid = "";
                    for (int index = 0; index < accessPoint.Bssid.Length; index++)
                    {
                        string hex = DebugHelpers.DebugHelpers.Hexadecimal(accessPoint.Bssid[index]);
                        bssid += hex.Substring(2, 2);
                        if (index != accessPoint.Bssid.Length - 1)
                        {
                            bssid += ":";
                        }
                    }
                    var network = new WifiNetwork(accessPoint.Ssid, bssid, NetworkType.Infrastructure, PhyType.Unknown, 
                        new NetworkSecuritySettings((NetworkAuthenticationType) accessPoint.AuthenticationMode, NetworkEncryptionType.Unknown),
                        accessPoint.PrimaryChannel, (NetworkProtocol) accessPoint.Protocols, accessPoint.Rssi);
                    networks.Add(network);
                }
            }
            else
            {
                accessPoints = null;
            }
            
            return (networks);
        }
        
        #endregion Methods
        
        #region POSIX Methods

        /// <summary>
        /// Provide a protocol-independent translation from an ANSI host name to an address
        /// </summary>
        /// <remarks>
        /// This method instructs the ESP32 to call the <i>getaddrinfo</i> method which will in turn
        /// call the equivalent LWIP method.
        ///
        /// See:
        /// <a href="http://www.nongnu.org/lwip/2_0_x/group__netdbapi.html#ga558191530d91c101621b49e43bd5bbf5">getaddrinfo</a>
        /// <a href="http://www.nongnu.org/lwip/2_0_x/lwip_2netdb_8h.html#af356989c172a51187e22b557f22d4165">lwip_getaddrinfo</a>
        /// </remarks>
        /// <param name="nodeName">Name of the server (node) name or numeric host address string (IP address).</param>
        /// <param name="serviceName">Service name or port number of the service.</param>
        /// <param name="hints">AddrInfo structure that contains hints about the type of socket the caller supports.</param>
        /// <param name="res">Pointer to a list of AddrInfo structures containing information about the host.</param>
        /// <returns>Error code if there is a problem, 0 if successful.</returns>
        public int GetAddrInfo(string nodeName, string serviceName, object hints, out object res)
        {
            var h = (AddrInfo) hints;

            var getAddrInfoRequest = new GetAddrInfoRequest { NodeName = nodeName, ServName = serviceName };
            var ai = Encoders.EncodeAddrInfo(h);
            getAddrInfoRequest.Hints = new byte[ai.Length];
            Array.Copy(ai, getAddrInfoRequest.Hints, ai.Length);
            getAddrInfoRequest.HintsLength = (UInt32) getAddrInfoRequest.Hints.Length;
            getAddrInfoRequest.Result = null;

            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.GetAddrInfo,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = Encoders.EncodeGetAddrInfoRequest(getAddrInfoRequest)
            };
            message.PayloadLength = (UInt32) message.Payload.Length;
            
            var response = _dispatcher.SendMessage(message);
            var addrInfoResponse = Encoders.ExtractGetAddrInfoResponse(response.Payload, 0);
            if (addrInfoResponse.Errno == 0)
            {
                res = Encoders.ExtractAddrInfo(addrInfoResponse.Res, 0);
            }
            else
            {
                res = null;
            }

            return (addrInfoResponse.Errno);
        }

        /// <summary>
        /// Create an endpoint for communication and return a socket handle (file descriptor)
        /// for the endpoint.
        /// </summary>
        /// <remarks>
        /// This method instructs the ESP32 to call the <i>socket</i> method which will in turn
        /// call the equivalent LWIP method.
        ///
        /// See:
        /// <a href="http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga862d8f4070c66dddb979540ce9ba6a83">socket</a>
        /// <a href="">socket</a>
        /// </remarks>
        /// <param name="domain">Specify the domain (communication protocol)</param>
        /// <param name="type">Communication type (semantics) to be used for this endpoint.</param>
        /// <param name="protocol">Protocol to be used.</param>
        /// <returns>Handle for the socket if successful, -1 otherwise.</returns>
        public int Socket(int domain, int type, int protocol)
        {
            var socketRequest = new SocketRequest {Domain = domain, Type = type, Protocol = protocol};
            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.Socket,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = Encoders.EncodeSocketRequest(socketRequest)
            };
            ;
            message.PayloadLength = (UInt32) message.Payload.Length;
            
            var response = _dispatcher.SendMessage(message);

            var socketResponse = Encoders.ExtractIntegerResponse(response.Payload, 0);
            return (socketResponse.Result);
        }

        /// <summary>
        /// Connect to the specified socket.
        /// </summary>
        /// <remarks>
        /// This method instructs the ESP32 to call the <i>socket</i> method which will in turn
        /// call the equivalent LWIP method.
        ///
        /// See:
        /// <a href="http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga862d8f4070c66dddb979540ce9ba6a83">socket</a>
        /// <a href="">socket</a>
        /// </remarks>
        /// <param name="socketHandle">ESP32 handle to the socket to connect to.</param>
        /// <param name="addr">Socket address information for the connect call.</param>
        /// <param name="addrlen">Size of the addr parameter.</param>
        /// <returns>0 if successful, -1 if this call fails.</returns>
        public int Connect(int socketHandle, SockAddr addr, int addrlen)
        {
            var connectRequest = new ConnectRequest { SocketHandle = socketHandle, Addr = Encoders.EncodeSockAddr(addr) };
            connectRequest.AddrLength = (UInt32) connectRequest.Addr.Length;
            connectRequest.AddrLen = addrlen;

            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.Connect,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = Encoders.EncodeConnectRequest(connectRequest)
            };
            message.PayloadLength = (UInt32) message.Payload.Length;
            
            var response = _dispatcher.SendMessage(message);

            var connectResponse = Encoders.ExtractIntegerResponse(response.Payload, 0);
            return (connectResponse.Result);
        }

        /// <summary>
        /// Free any previously allocated addrinfo structure and pointers.
        /// </summary>
        /// <remarks>
        /// This method instructs the ESP32 to call the <i>socket</i> method which will in turn
        /// call the equivalent LWIP method.
        ///
        /// See:
        /// <a href="http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga862d8f4070c66dddb979540ce9ba6a83">socket</a>
        /// <a href="http://www.nongnu.org/lwip/2_0_x/lwip_2netdb_8h.html#a7f65ff5982a0743849a644ef2cd15ef5">lwip_freeaddrinfo</a>
        /// </remarks>
        /// <param name="addrInfoPointer">UInt32 value that represents the pointer to the addrinfo object on the ESP32.</param>
        public void FreeAddrInfo(UInt32 addrInfoPointer)
        {
            var freeAddrInfoRequest = new FreeAddrInfoRequest { AddrInfoAddress = addrInfoPointer };

            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.FreeAddrInfo,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = Encoders.EncodeFreeAddrInfoRequest(freeAddrInfoRequest)
            };
            message.PayloadLength = (UInt32) message.Payload.Length;
            
            _dispatcher.SendMessage(message);
        }

        /// <summary>
        /// Set the options for the specified socket.
        /// </summary>
        /// <remarks>
        /// This method instructs the ESP32 to call the <i>setsockopt</i> method which will in turn
        /// call the equivalent LWIP method.
        ///
        /// See:
        /// <a href="http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga115d74cd1953e7bafc2e34157c697df1">setsockopt</a>
        ///
        /// The only supported socket level type is SolSocket 
        ///
        /// According to the ESP32 documentation, the following socket option types are NOT supported:
        ///     - SoDebug
        ///     - SoDontRoute
        ///     - SoUseLoopBack
        ///     - SoOobInline
        ///     - SoReusePort
        ///     - SoSndBuf
        ///     - SoSndLoWat
        ///     - SoRcvLoWat
        /// </remarks>
        /// <param name="socketHandle">ESP32 handle for the socket to be operated on.</param>
        /// <param name="level">Currently only SolSocket is supported for this method.</param>
        /// <param name="optionName">Option to change (see remarks above).</param>
        /// <param name="optionValue">Data relevant to the option name being changed.</param>
        /// <param name="optionLength">Size of the optionValue parameter in bytes.</param>
        /// <returns>-1 if the setsockopt call on the ESP32 fails.</returns>
        public int SetSockOpt(int socketHandle, SocketLevelType level, SocketOptionType optionName, object optionValue, UInt32 optionLength)
        {
            var setSockOptRequest = new SetSockOptRequest { SocketNumber = socketHandle, Level = (int)level, OptionName = (int)optionName };
            switch (optionName)
            {
                case SocketOptionType.SoAcceptConn:         /* Socket has had listen() */
                    break;
                case SocketOptionType.SoLinger:             /* Linger on close if data present */
                    break;
                case SocketOptionType.SoDontLinger:         /* Don't linger on close if data present */
                    break;
                case SocketOptionType.SoRcvBuf:             /* Set the receive buffer size */
                    break;
                case SocketOptionType.SoSndTimeO:           /* Set the send timeout */
                    break;
                case SocketOptionType.SoRcvTimeO:           /* Set the receive timeout */
                    var timeValue = (TimeVal) optionValue;
                    setSockOptRequest.OptionValue = Encoders.EncodeTimeVal(timeValue);
                    setSockOptRequest.OptionValueLength = (UInt32) setSockOptRequest.OptionValue.Length;
                    setSockOptRequest.OptionLen = 0;        //    This will be calculated in the ESP driver.
                    break;
                case SocketOptionType.SoError:              /* Get error status and clear */
                    break;
                case SocketOptionType.SoType:               /* Get socket type */
                    break;
                case SocketOptionType.SoNoCheck:            /* Don't create UDP checksum */
                    break;
            }

            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.SetSockOpt,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = Encoders.EncodeSetSockOptRequest(setSockOptRequest)
            };
            message.PayloadLength = (UInt32) message.Payload.Length;
            
            var response = _dispatcher.SendMessage(message);
            var setSockOptResponse = Encoders.ExtractIntegerAndErrnoResponse(response.Payload, 0);
            var resultCode = setSockOptResponse.Result;
            if (resultCode < 0)
            {
                resultCode = setSockOptResponse.Errno;
            }
            return(resultCode);
        }

        /// <summary>
        /// Write the specified number of bytes to the socket.
        /// </summary>
        /// <remarks>
        /// This method instructs the ESP32 to call the <i>write</i> method which will in turn
        /// call the equivalent LWIP method.
        ///
        /// See:
        /// <a href="http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga0a651eb5fb5e6127f5e5153ce2251f3d">socket</a>
        /// </remarks>
        /// <param name="socketHandle">ESP32 handle for the socket to write the data to.</param>
        /// <param name="buffer">Data to be written to the socket.</param>
        /// <param name="count">Number of bytes to write to the socket.</param>
        /// <returns>Number of bytes written to the socket.</returns>
        public int Write(int socketHandle, byte[] buffer, int count)
        {
            var request = new WriteRequest { SocketHandle = socketHandle, Buffer = buffer, Count = count, BufferLength = (uint)buffer.Length };

            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.Write,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = Encoders.EncodeWriteRequest(request)
            };
            message.PayloadLength = (UInt32) message.Payload.Length;
            
            var response = _dispatcher.SendMessage(message);
            var setSockOptResponse = Encoders.ExtractIntegerAndErrnoResponse(response.Payload, 0);
            var resultCode = setSockOptResponse.Result;
            //
            //    TODO: Do something about errno.
            //
            if (resultCode < 0)
            {
                resultCode = setSockOptResponse.Errno;
            }
            return(resultCode);
        }

        /// <summary>
        /// Read the specified number of bytes from the socket and place them in the buffer.
        /// </summary>
        /// <remarks>
        /// This method instructs the ESP32 to call the <i>read</i> method which will in turn
        /// call the equivalent LWIP method.
        ///
        /// See:
        /// <a href="http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga822040573319cf87bfe6758d511be57f">read</a>
        /// </remarks>
        /// <param name="socketHandle">ESP32 handle to the socket to be read.</param>
        /// <param name="buffer">Buffer to hold the results of the read.</param>
        /// <param name="count">Number of bytes in the buffer.</param>
        /// <returns>Number of bytes read from the socket.</returns>
        public int Read(int socketHandle, byte[] buffer, int count)
        {
            var request = new ReadRequest { SocketHandle = socketHandle, Count = count };

            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.Read,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = Encoders.EncodeReadRequest(request)
            };
            message.PayloadLength = (UInt32) message.Payload.Length;
            
            var response = _dispatcher.SendMessage(message);
            var readResponse = Encoders.ExtractReadResponse(response.Payload, 0);
            var amountRead = readResponse.Result;
            //
            //    TODO: Do something about errno.
            //
            Array.Clear(buffer, 0, count);
            if (amountRead > 0)
            {
                Array.Copy(readResponse.Buffer, buffer, amountRead);
            }

            if (amountRead < 0)
            {
                DebugHelpers.DebugHelpers.LogMessage(true, $"    Error number: {readResponse.Errno}");
            }
            return(amountRead);
        }

        /// <summary>
        /// Call the close method on the ESP32 to close the specified socket handle.
        /// </summary>
        /// <remarks>
        /// This method instructs the ESP32 to call the <i>close</i> method which will in turn
        /// call the equivalent LWIP method.
        ///
        /// See:
        /// <a href="http://www.nongnu.org/lwip/2_0_x/group__socket.html#ga4ef17e85ec4d3acdcee5ce23f8ed93c4">socket</a>
        /// </remarks>
        /// <param name="socketHandle">ESP32 handle to the socket to be closed.</param>
        /// <returns>0 for success, -1 for failure.</returns>
        public int Close(int socketHandle)
        {
            var request = new CloseRequest { SocketHandle = socketHandle };

            var message = new Message
            {
                Interface = (byte)Esp32Interfaces.WiFi,
                Function = (uint)WiFiFunction.Close,
                MessageID = _dispatcher.GetNextMessageId(),
                MessageType = (byte)MessageTypes.Data,
                Payload = Encoders.EncodeCloseRequest(request)
            };
            message.PayloadLength = (UInt32) message.Payload.Length;
            
            var response = _dispatcher.SendMessage(message);
            var closeResponse = Encoders.ExtractIntegerAndErrnoResponse(response.Payload, 0);
            return (closeResponse.Result);
        }

        #endregion POSIX Methods
    }
}
