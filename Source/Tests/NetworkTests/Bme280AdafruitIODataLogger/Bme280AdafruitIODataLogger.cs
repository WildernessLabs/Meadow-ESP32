using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Foundation.Sensors.Atmospheric;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class Bme280AdafruitIODataLogger : App<F7Micro, Bme280AdafruitIODataLogger>
    {
        /// <summary>
        /// Default constructor.
        /// </summary>
        public Bme280AdafruitIODataLogger()
        {
            Log("Starting watchdog.");
            //MeadowOS.WatchdogEnable(20000);
            //StartWatchdogThread(15000);

            Log("Connecting to network.");
            Device.InitWiFiAdapter().Wait();
            if (Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception("Cannot connect to network, application halted.");
            }
            Log("WiFi connection completed.");
        }

        public void Log(string message)
        {
            StringBuilder output = new StringBuilder();
            output.Append(DateTime.Now.ToString("HH:mm:ss"));
            output.Append(": ");
            output.Append(message);
            Console.WriteLine(output.ToString());
        }

        private void StartWatchdogThread(int interval)
        {
            MeadowOS.WatchdogReset();
            Thread t = new Thread(() =>
            {
                while (true)
                {
                    Thread.Sleep(interval);
                    MeadowOS.WatchdogReset();
                }
            });
            t.Start();
        }

        public void RunTest()
        {
            byte[] bytes = new byte[2048];
            Bme280 bme280;

            //MeadowOS.WatchdogReset();
            var i2c = Device.CreateI2cBus();
            bme280 = new Bme280(i2c, Bme280.I2cAddress.Adddress0x77);
            bme280.StartUpdating();

            Log("Looking up DNS.");
            IPHostEntry ipHostInfo = Dns.GetHostEntry("io.adafruit.com");
            IPAddress ipAddress = ipHostInfo.AddressList[0];
            IPEndPoint remoteEP = new IPEndPoint(ipAddress, 80);

            int dataSetCount = 0;
            StringBuilder data = new StringBuilder();
            StringBuilder temperaturePreamble = new StringBuilder();
            temperaturePreamble.Append("POST /api/v2/nevyn/feeds/airtemperature/data HTTP/1.1\r\n");
            temperaturePreamble.AppendFormat("X-AIO-Key: {0}\r\n", Secrets.APIO_KEY);
            temperaturePreamble.Append("Content-Type: application/json; charset=utf-8\r\n");

            StringBuilder humidityPreamble = new StringBuilder();
            humidityPreamble.Append("POST /api/v2/nevyn/feeds/humidity/data HTTP/1.1\r\n");
            humidityPreamble.AppendFormat("X-AIO-Key: {0}\r\n", Secrets.APIO_KEY);
            humidityPreamble.Append("Content-Type: application/json; charset=utf-8\r\n");

            StringBuilder presurePreamble = new StringBuilder();
            presurePreamble.Append("POST /api/v2/nevyn/feeds/airpressure/data HTTP/1.1\r\n");
            presurePreamble.AppendFormat("X-AIO-Key: {0}\r\n", Secrets.APIO_KEY);
            presurePreamble.Append("Content-Type: application/json; charset=utf-8\r\n");

            string host = "Host: io.adafruit.com\r\n\r\n";
            StringBuilder temperatureData = new StringBuilder();
            StringBuilder humidityData = new StringBuilder();
            StringBuilder pressureData = new StringBuilder();

            Log("Creating socket with server");
            Socket sender = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp);
            sender.SendTimeout = 3000;
            sender.ReceiveTimeout = 3000;
            sender.Connect(remoteEP);

            #region Send data.

            int bytesSent;
            int bytesReceived;
            //
            //  Temperature
            //
            while (true)
            {
                //MeadowOS.WatchdogReset();
                dataSetCount++;
                Log($"Logging dataset {dataSetCount}");
                Log("Sending temperature data.");
                temperatureData.Append("{\"value\":\"");
                temperatureData.AppendFormat("{0:F2}", bme280.Temperature - 2.0F);
                temperatureData.Append("\"}");
                data.Append(temperaturePreamble);
                data.AppendFormat("Content-Length: {0}\r\n", temperatureData.Length);
                data.Append(host);
                data.Append(temperatureData);
                bytesSent = sender.Send(Encoding.ASCII.GetBytes(data.ToString()));
                bytesReceived = sender.Receive(bytes);
                data.Clear();
                temperatureData.Clear();
                //
                //  Humidity
                //
                Log("Sending humidity data.");
                humidityData.Append("{\"value\":\"");
                humidityData.AppendFormat("{0:F2}", bme280.Humidity);
                humidityData.Append("\"}");
                data.Append(humidityPreamble);
                data.AppendFormat("Content-Length: {0}\r\n", humidityData.Length);
                data.Append(host);
                data.Append(humidityData);
                bytesSent = sender.Send(Encoding.ASCII.GetBytes(data.ToString()));
                bytesReceived = sender.Receive(bytes);
                data.Clear();
                humidityData.Clear();
                //
                //  Air pressure
                //
                Log("Sending air pressure data.");
                pressureData.Append("{\"value\":\"");
                pressureData.AppendFormat("{0:F0}", bme280.Pressure);
                pressureData.Append("\"}");
                data.Append(presurePreamble);
                data.AppendFormat("Content-Length: {0}\r\n", pressureData.Length);
                data.Append(host);
                data.Append(pressureData);
                bytesSent = sender.Send(Encoding.ASCII.GetBytes(data.ToString()));
                bytesReceived = sender.Receive(bytes);
                data.Clear();
                pressureData.Clear();

                Thread.Sleep(10000);

                #endregion

                Log($"Memory available: {GC.GetTotalMemory(false)}");
            }
        }
    }
}
