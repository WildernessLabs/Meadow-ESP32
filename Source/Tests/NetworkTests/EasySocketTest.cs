using Meadow;
using Meadow.Devices;
using Meadow.Foundation;
using Meadow.Foundation.Leds;
using Meadow.Gateway.WiFi;
using Meadow.Gateways;
using System;
using System.Net.Sockets;
using System.Threading;
using System.Collections.Generic;
using System.Net;
using System.Text;

//
//  Taken from the public slack forum.
//
//  Code files added to Support channel by Unreal on 9th Aug 2021.
//
//  Unreal:palm_tree:  07:14
//  replied to a thread:
//  This is the same as https://wildernesslabspublic.slack.com/archives/CQWD660E8/p1628433834013700.
//  The Socket I wrote cannot be used. The problem lies in Socket.Connect();
//  it throw "network subsystem is down"
//
//  MS: Checked out on 14 Sept and this problem could not be reproduced.
//

namespace MeadowApp
{
    public struct Url
    {
        public string Host;
        public int Port;
        public string File;
        public string Body;
    }

    class EasySocket
    {
        public static string Get(Url url, Encoding encode)
        {
            var data = $"GET http://{url.Host}:{url.Port}/{url.File} HTTP/1.1\r\n" +
                $"Host:{url.Host}:{url.Port}\r\n" +
                $"User-Agent: Meadow F7 Iot Device" +
                $"Connection:Close\r\n\r\n";
            return GetResponse(url, data, encode);
        }

        public static string Post(Url url, Encoding encode)
        {
            var data = $"POST http://{url.Host}:{url.Port}/{url.File} HTTP/1.1\r\n" +
                $"Host:{url.Host}:{url.Port}\r\n" +
                $"Content-Length:{url.Body.Length}\r\n" +
                $"Content-Type:*/*\r\n" +
                $"User-Agent: Meadow F7 Iot Device" +
                $"Connection:Close\r\n\r\n" +
                $"{url.Body}";
            return GetResponse(url, data, encode);
        }

        private static string GetResponse(Url url,string request, Encoding encode)
        {
            var data = encode.GetBytes(request);
            Span<byte> recv = stackalloc byte[1024];
            int i = 0;

            using (var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp))
            {
                Console.WriteLine("Make Socket");
                try
                {
                    Console.WriteLine("Make IP");
                    var ip = IPAddress.Parse(url.Host);
                    Console.WriteLine("Socket Connection");
                    socket.Connect(ip, url.Port);
                    if (socket.Connected)
                    {
                        Console.WriteLine($"Is Connected {socket.Connected}\r\nSend Data");
                        socket.Send(data, data.Length, 0);


                        while ((i = socket.Receive(recv)) > 0) { }
                    }
                    socket.Close();
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.Message + "\r\n" + ex.StackTrace);
                }
                return encode.GetString(recv);
            }
        }
    }

    public class EasySocketTest : App<F7Micro, EasySocketTest>
    {
        public EasySocketTest()
        {
        }

        public void RunTest()
        {
            Console.WriteLine(Device.Coprocessor.Status);
            Console.WriteLine("Set Antenna");
            Device.SetAntenna(AntennaType.OnBoard);
            Console.WriteLine("Init WIFI");
            Device.InitWiFiAdapter().Wait(5000);

            Device.WiFiAdapter.WiFiDisconnected += WiFiAdapter_WiFiDisconnected;

            Console.WriteLine("Con WIFI");
            WiFiAdapter_WiFiDisconnected(null, null);

            var url = new Url()
            {
                Host = "192.168.4.1",
                Port = 80,
                File = "GetRTC/"
            };
            Console.WriteLine(EasySocket.Get(url, System.Text.Encoding.UTF8));
        }

        private void WiFiAdapter_WiFiDisconnected(object sender, EventArgs e)
        {
            Device.WiFiAdapter.StartWiFiInterface();
            Console.WriteLine("Wifi Disconnect");
            var tf = Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).Result.ConnectionStatus;
            F7Micro.DeviceInformation information = Device.GetDeviceInformation();
            Console.WriteLine(Device.WiFiAdapter.IpAddress);
            Console.WriteLine(
                $"{Environment.NewLine}Wifi IsConnected: {Device.WiFiAdapter.IsConnected}" +
                $"{Environment.NewLine}Name: {information.DeviceName}" +
                $"{Environment.NewLine}IP: {Device.WiFiAdapter.IpAddress}" +
                $"{Environment.NewLine}Access Point: {Device.WiFiAdapter.DefaultAcessPoint}" +
                $"{Environment.NewLine}Gateway: {Device.WiFiAdapter.Gateway}" +
                $"{Environment.NewLine}Mask: {Device.WiFiAdapter.SubnetMask}"
                );
        }
    }
}
