using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Foundation;
using Meadow.Foundation.Leds;
using Meadow.Gateway.WiFi;
using Meadow.Gateways;
// using Newtonsoft.Json;

namespace MeadowApp
{
    public class OutOfMemoryTest : App<F7Micro, OutOfMemoryTest>
    {
        public async Task RunAsync()
        {
            var rgbPwmLed = new RgbPwmLed(Device, Device.Pins.OnboardLedRed, Device.Pins.OnboardLedGreen, Device.Pins.OnboardLedBlue);
            // var configFile = await File.ReadAllTextAsync("/meadow0/appSettings.Local.json");
            // var configuration = JsonConvert.DeserializeObject<Dictionary<string, object>>(configFile) ??
            //                     throw new InvalidOperationException("Could not parse configuration");
            // var networkName = configuration["ssid"].ToString();
            // var networkPassword = configuration["password"].ToString();
            try
            {
                rgbPwmLed.SetColor(Color.Blue);
                Console.WriteLine("Initialize WiFi adapter...");
                await Device.InitWiFiAdapter().ConfigureAwait(false);
                if (Device.WiFiAdapter == null)
                    throw new InvalidOperationException("WIFI null");
                // this works with latest OS version (caused connection failure on earlier version)
                Device.WiFiAdapter.SetAntenna(AntennaType.External);
                Console.WriteLine($"Connecting to {Secrets.WIFI_NAME}...");
                ConnectionStatus connectionStatus;
                while ((connectionStatus = (await Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD).ConfigureAwait(false)).ConnectionStatus) != ConnectionStatus.Success)
                {
                    rgbPwmLed.SetColor(Color.Red);
                    Console.WriteLine($"WiFi connect failed with {connectionStatus}");
                    await Task.Delay(TimeSpan.FromSeconds(5));
                    MeadowOS.CurrentDevice.Reset();
                }
                Console.WriteLine("Network connected...");
            }
            catch (Exception ex)
            {
                rgbPwmLed.SetColor(Color.Red);
                Console.WriteLine(ex);
            }
            var counter = 0;
            var stopwatch = new Stopwatch();
            // moving this inside the loop will cause the requests to pass faster but why??
            using var client = new HttpClient();
            while (true)
            {
                try
                {
                    rgbPwmLed.SetColor(Color.LightYellow);
                    Console.WriteLine($"Request {++counter}");
                    stopwatch.Restart();
                    // normally on desktop I wouldn't use the dispose on the response
                    using var response = await client.GetAsync("https://httpbin.org/status/200").ConfigureAwait(false);
                    stopwatch.Stop();
                    Console.WriteLine($"Elapsed: {stopwatch.Elapsed.TotalSeconds}s, Status: {response.StatusCode}, ContentLength: {response.Content.Headers.ContentLength}");
                    rgbPwmLed.Stop();
                    rgbPwmLed.SetColor(Color.Green);
                    await Task.Delay(TimeSpan.FromSeconds(2)).ConfigureAwait(false);
                }
                catch (Exception ex)
                {
                    rgbPwmLed.SetColor(Color.Red);
                    Console.WriteLine(ex);
                }
                Console.WriteLine("Cycling...");
            }
        }
    }
}