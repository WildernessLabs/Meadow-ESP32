using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Coprocessor.F7Micro;
using Meadow.Coprocessor.F7Micro.Messages;
using Meadow.Gateway.WiFi;

namespace BasicNetworkTest
{
    public class HttpGetTest : App<F7Micro, HttpGetTest>
    {
        #region Private member variables (fields)
        
        /// <summary>
        /// Button that can be used to trigger a test.
        /// </summary>
        private DigitalInputPort _button;
        
        /// <summary>
        /// ESP32 UART Rx port.
        /// </summary>
        IDigitalInputPort _esp32UartRx;
        
        /// <summary>
        /// ESP32 UART Tx port.
        /// </summary>
        IDigitalInputPort _esp32UartTx;

        /// <summary>
        /// Network coprocessor.
        /// </summary>
        private readonly F7MicroCoprocessor _f7MicroCoprocessor;
        
        /// <summary>
        /// Message dispatcher responsible for sending messages to the ESP32 and also passing
        /// any messages from the ESP32 to the relevant interface on the STM32.
        /// </summary>
        private readonly MessageDispatcher _dispatcher;
        
        #endregion Private member variables (fields)
        
        #region Constructors
        
        /// <summary>
        /// Default constructor.
        /// </summary>
        public HttpGetTest()
        {
            //
            //  The UART pins need instantiating as inputs in order to allow the ESP32 debug information to
            //  appear on any serial console attached to the ESP32 chip.
            //
            _esp32UartRx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_RX, resistorMode: ResistorMode.Disabled);
            _esp32UartTx = (IDigitalInputPort) Device.CreateDigitalInputPort(Device.Pins.ESP_UART_TX, resistorMode: ResistorMode.Disabled);

            _dispatcher = new MessageDispatcher(Device,
                mosi: Device.Pins.ESP_MOSI, miso: Device.Pins.ESP_MISO, clock: Device.Pins.ESP_CLK, chipSelect: Device.Pins.ESP_CS,
                bootPin: Device.Pins.ESP_BOOT, resetPin: Device.Pins.ESP_RST);

            _f7MicroCoprocessor = new F7MicroCoprocessor(_dispatcher);
            //
            //  Wire up the button to run the tests on demand.
            //
            _button = (DigitalInputPort)Device.CreateDigitalInputPort(Device.Pins.D14,
                interruptMode: InterruptMode.EdgeFalling, resistorMode: ResistorMode.PullUp, debounceDuration: 500);
            _button.Changed += _button_Changed;
            //
            //  Now run the test once.  Further tests can be run by pressing the button on D14.
            //
            RunTest();
        }
        
        #endregion Constructors
        
        #region Events
        
                
        /// <summary>
        /// Run the test when the button is pressed.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void _button_Changed(object sender, DigitalInputPortEventArgs e)
        {
            RunTest();
        }

        #endregion Events
        
        #region Methods

        /// <summary>
        /// Run the HttpGet test.
        /// </summary>
        private void RunTest()
        {
            DebugHelpers.DebugHelpers.LogMessage(true, "Starting network"); 
            
            var ipAddress = _f7MicroCoprocessor.StartNetwork(Secrets.WIFI_NAME, Secrets.WIFI_PASSOWRD);
            DebugHelpers.DebugHelpers.LogMessage(true, $"Obtained IP address: {ipAddress.ToString()}");
            //
            //    getaddrinfo
            //
            var hints = new AddrInfo();
            hints.Family = (int) AddressFamilyType.AfInet;
            hints.SocketType = (int) SocketType.Stream;
            hints.AddrLength = 0;
            hints.CanonName = "";

            DebugHelpers.DebugHelpers.LogMessage(true, "Requesting lookup for www.google.com:80");
            var errorCode = _f7MicroCoprocessor.GetAddrInfo("www.google.com", "80", hints, out var res);
            DebugHelpers.DebugHelpers.LogMessage(true, $"    Error code {errorCode}");
            if (errorCode != 0)
            {
                DebugHelpers.DebugHelpers.LogMessage(true, $"Exiting test");
                return;
            }
            var getAddrInfoResult = (AddrInfo) res;
            //
            //    socket
            //            
            DebugHelpers.DebugHelpers.LogMessage(true, "Getting socket handle");
            int socketHandle = _f7MicroCoprocessor.Socket(getAddrInfoResult.Family, getAddrInfoResult.SocketType, 0);
            DebugHelpers.DebugHelpers.LogMessage(true, $"    Socket handle: {socketHandle}");
            if (socketHandle < 0)
            {
                _f7MicroCoprocessor.FreeAddrInfo(getAddrInfoResult.MyHeapAddress);
                DebugHelpers.DebugHelpers.LogMessage(true, $"Exiting test");
                return;
            }
            //
            //    connect
            //
            DebugHelpers.DebugHelpers.LogMessage(true, "Opening connection (connect)");
            SockAddr sockAddr = Encoders.ExtractSockAddr(getAddrInfoResult.Addr, 0);
            if (_f7MicroCoprocessor.Connect(socketHandle, sockAddr, (int) getAddrInfoResult.AddrLen) < 0)
            {
//                close(socketHandle);
                _f7MicroCoprocessor.FreeAddrInfo(getAddrInfoResult.MyHeapAddress);
                DebugHelpers.DebugHelpers.LogMessage(true, $"Exiting test");
                return;
            }
            DebugHelpers.DebugHelpers.LogMessage(true, "    Connected.");
            //
            //    freeaddrinfo
            //
            DebugHelpers.DebugHelpers.LogMessage(true, "Freeing address information resources (freeaddrinfo)");
            _f7MicroCoprocessor.FreeAddrInfo(getAddrInfoResult.MyHeapAddress);
            DebugHelpers.DebugHelpers.LogMessage(true, "    Complete.");
            //
            //    setsockopt
            //
            DebugHelpers.DebugHelpers.LogMessage(true, "Setting socket options (setsockopt)");
            var tv = new TimeVal();
            tv.TvSec = 5;
            tv.TvUsec = 0;
            if (_f7MicroCoprocessor.SetSockOpt(socketHandle, SocketLevelType.SolSocket, SocketOptionType.SoRcvTimeO, tv, 0) != 0)
            {
//                close(socketHandle);
                DebugHelpers.DebugHelpers.LogMessage(true, $"Exiting test");
                return;
            }
            DebugHelpers.DebugHelpers.LogMessage(true, "    Timeout set for socket.");
        }
        
        #endregion Methods
    }
}
