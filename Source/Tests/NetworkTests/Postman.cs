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

namespace MeadowApp
{
    public class Postman : App<F7Micro, Postman>
    {
        public async Task RunAsync()
        {
            var rgbPwmLed = new RgbPwmLed(
                Device,
                Device.Pins.OnboardLedRed,
                Device.Pins.OnboardLedGreen,
                Device.Pins.OnboardLedBlue);
            var counter = 0;
            var stopwatch = new Stopwatch();
            Console.WriteLine($"Before request loop. GetTotalMemory: {GC.GetTotalMemory(false) / 1000} KB");
            // moving this inside the loop will cause the requests to pass faster but why??
            using var client = new HttpClient();
            while (true)
            {
                try
                {
                    //using var client = new HttpClient();
                    rgbPwmLed.SetColor(Color.LightYellow);
                    Console.WriteLine($"Request {++counter}");
                    stopwatch.Restart();
                    // normally on desktop I wouldn't use the dispose on the response
                    using var response = await client.GetAsync("https://postman-echo.com/get?foo1=bar1&foo2=bar2").ConfigureAwait(false);
                    stopwatch.Stop();
                    Console.WriteLine($"Elapsed: {stopwatch.Elapsed.TotalSeconds}s, Status: {response.StatusCode}, GetTotalMemory: {GC.GetTotalMemory(false)/1000}KB");
                    rgbPwmLed.Stop();
                    rgbPwmLed.SetColor(Color.Green);
                    await Task.Delay(TimeSpan.FromSeconds(2)).ConfigureAwait(false);
                }
                catch (Exception ex)
                {
                    rgbPwmLed.SetColor(Color.Red);
                    Console.WriteLine(ex);
                    Console.WriteLine($"GetTotalMemory: {GC.GetTotalMemory(false) / 1000}KB");
                    break;
                }
                Console.WriteLine("Cycling...");
            }
        }
    }
}