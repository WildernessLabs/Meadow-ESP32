using System;
using System.Net;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateway.WiFi;
using System.Threading;
using System.Threading.Tasks;
using System.Net.Http;
using Meadow.Gateways;
using System.Collections.Generic;

namespace MeadowApp
{
    /// <summary>
    /// Reproduce the OAUTH issue 
    /// </summary>
    /// <remarks>
    /// 
    /// Issue Title: HttpClient fails with 'No such host is known' when trying to POST to MSFT Oauth endpoint.
    /// 
    /// Issue Link: https://github.com/WildernessLabs/Meadow_Issues/issues/141
    /// 
    /// Description:
    /// The Meadow app crashes when trying to POST to the aforementioned server. Network package sniffer shows
    /// a DNS resolve request-response pair but the actual server is not contacted. If I change the server name
    /// in the URL to, let's say 'wildernesslabs.co', the app contacts the WildernessLabs server and returns 
    /// with HTTP 403 - so the nw communication works in this case.
    /// 
    /// </remarks>
    public class OauthTest : App<F7Micro, OauthTest>
    {
        public OauthTest()
        {
            Console.WriteLine("Initialising WiFi Adapter object.");

            Device.InitWiFiAdapter().Wait();
        }

        /// <summary>
        /// Run the test.
        /// </summary>
        public async Task RunTest()
        {
            Console.WriteLine($"Connecting to WiFi Network {Secrets.WIFI_NAME}");
            var connectionResult = await Device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
            if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
            {
                throw new Exception($"Cannot connect to network: {connectionResult.ConnectionStatus}");
            }

            Console.WriteLine("Getting data.");

            var deviceAuthorizationRequestContent = new Dictionary<string, string>();
            deviceAuthorizationRequestContent.Add("client_id", "6731de76-14a6-49ae-97bc-6eba6914391e");
            deviceAuthorizationRequestContent.Add("scope", "https://graph.microsoft.com/mail.read api://");
            deviceAuthorizationRequestContent.Add("response_type", "code");
            deviceAuthorizationRequestContent.Add("redirect_uri", "http://localhost/");
            deviceAuthorizationRequestContent.Add("response_more", "query");
            deviceAuthorizationRequestContent.Add("state", "12345");
            deviceAuthorizationRequestContent.Add("code_challenge", "YTFjNjI1OWYzMzA3MTI4ZDY2Njg5M2RkNmVjNDE5YmEyZGRhOGYyM2IzNjdmZWFhMTQ1ODg3NDcxY2Nl");
            deviceAuthorizationRequestContent.Add("code_challenge_method", "S256");
            var deviceAuthorizationRequestUri = $"https://login.microsoftonline.com/common/oauth2/v2.0/authorize";

            var authFormEncodedContent = new FormUrlEncodedContent(deviceAuthorizationRequestContent);

            using (HttpClient client = new HttpClient())
            {
                client.Timeout = new TimeSpan(0, 5, 0);
                Console.WriteLine("Preparing content.");
                var content = new StringContent(authFormEncodedContent.ReadAsStringAsync().Result, Encoding.UTF8, "application/json");

                Console.WriteLine("Posting request.");
                var response = client.PostAsync(deviceAuthorizationRequestUri, content).Result;

                Console.WriteLine("Checking response code.");
                response.EnsureSuccessStatusCode();

                Console.WriteLine("Reading response as string.");
                Console.WriteLine(response.Content.ReadAsStringAsync().Result);
            }
        }
    }
}
