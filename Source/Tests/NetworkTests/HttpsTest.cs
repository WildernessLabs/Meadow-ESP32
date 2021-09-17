using System;
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

//
//  Pekka Heikura 8 Sep at 11:53
//  I've been running my HTTPS test again with the latest b5.3 and hitting the issues again.
//  The device just stops responding after few https requests to "https://postman-echo.com/get?foo1=bar1&foo2=bar2".
//  FYI @Alexis Christoforides.  Also feels like it gets stuck connecting to Wifi more often.
//
//  1 reply
//
//  Pekka Heikura  5 days ago
//  The test app is here https://github.com/pekkah/meadow-libs. This one
//  https://github.com/pekkah/meadow-libs/tree/main/src/Verify.Wifi. Read the README at root of the repo for instructions.
//
namespace MeadowApp
{
    public class HttpsTest : App<F7Micro, HttpsTest>
    {
        public HttpsTest()
        {
        }

        public async Task RunAsyncTest()
        {
            var rgbPwmLed = new RgbPwmLed(
                Device,
                Device.Pins.OnboardLedRed,
                Device.Pins.OnboardLedGreen,
                Device.Pins.OnboardLedBlue);

            try
            {
                rgbPwmLed.SetColor(Color.Blue);
                Console.WriteLine("Initialize WiFi adapter...");
                await Device.InitWiFiAdapter()
                    .ConfigureAwait(false);

                if (Device.WiFiAdapter == null)./bu
                    throw new InvalidOperationException("WIFI null");

                // this works with latest OS version (caused connection failure on earlier version)
                Device.WiFiAdapter.SetAntenna(AntennaType.OnBoard);

                Console.WriteLine($"Connecting to {Secrets.WIFI_NAME}...");
                ConnectionStatus connectionStatus;
                while ((connectionStatus = (await Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD)
                           .ConfigureAwait(false)).ConnectionStatus)
                       != ConnectionStatus.Success)
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
                return;
            }
            
            var counter = 0;
            var stopwatch = new Stopwatch();
            Console.WriteLine($"Before request loop. GetTotalMemory: {GC.GetTotalMemory(false) / 1000}KB");

            // moving this inside the loop will cause the requests to pass faster but why??
            using var client = new HttpClient();
            while (true)
            {
                try
                {
                    // comment out the above client to use new client per request
                    // using var client = new HttpClient();
                    rgbPwmLed.SetColor(Color.LightYellow);
                    Console.WriteLine($"Request {++counter}");
                    stopwatch.Restart();

                    // normally on desktop I wouldn't use the dispose on the response
                    using var response = await client.GetAsync("https://postman-echo.com/get?foo1=bar1&foo2=bar2").ConfigureAwait(false);
                    //
                    //  Substituting the line below for the one above means the system will make
                    //  significantly more requests.
                    //
                    // using var response = await client.GetAsync("http://postman-echo.com/get?foo1=bar1&foo2=bar2").ConfigureAwait(false);

                    stopwatch.Stop();
                    Console.WriteLine($"Elapsed: {stopwatch.Elapsed.TotalSeconds}s, Status: {response.StatusCode}, GetTotalMemory: {GC.GetTotalMemory(false) / 1000}KB");
                    rgbPwmLed.Stop();
                    rgbPwmLed.SetColor(Color.Green);
                    await Task.Delay(TimeSpan.FromSeconds(5))
                        .ConfigureAwait(false);
                }
                catch (HttpRequestException x)
                {
                    rgbPwmLed.SetColor(Color.Yellow);
                    Console.WriteLine("Request failed {0}. Continuing.", x);
                }
                catch (Exception ex)
                {
                    rgbPwmLed.SetColor(Color.Red);
                    Console.WriteLine(ex);
                    Console.WriteLine($"In exception. GetTotalMemory: {GC.GetTotalMemory(false) / 1000}KB");
                    break;
                }

                Console.WriteLine("Cycling...");
            }
        }
    }
}
