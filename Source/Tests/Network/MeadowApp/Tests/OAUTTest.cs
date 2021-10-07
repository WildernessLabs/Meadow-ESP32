using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Meadow.Devices;
using Meadow.Gateway.WiFi;

namespace MeadowApp
{
    public class OauthTest : IMeadowTest
    {
        /// <summary>
        /// F7Micro device object to allow access to the hardware.
        /// </summary>
        F7Micro _device;

        /// <summary>
        /// Message dispatcher used to exchange messages between the test system and the host computer.
        /// </summary>
        MessageDispatcher _messageDispatcher;

        /// <summary>
        /// Lock variable for the system state variables.
        /// </summary>
        private object _lock = new object();

        /// <summary>
        /// Indicate that this is an asynchronous test.
        ///
        /// The value of the test indicates which Execute method should be run:
        ///     true - ExecuteAsync should be called
        ///     false - Execute should be called.
        ///
        /// The invalid method should throw a NotImplementedException.
        /// </summary>
        public bool AsyncTest { get { return (true); } }

        /// <summary>
        /// Default constructor is private to prevent it being used.
        /// </summary>
        private OauthTest() { }

        /// <summary>
        /// Constructor for the test.
        /// </summary>
        /// <param name="device">Device object giving access to the underlying hardware.</param>
        /// <param name="messageDispatcher">Message dispatcher used to communicate with the host computer.</param>
        public OauthTest(F7Micro device, MessageDispatcher messageDispatcher)
        {
            _device = device;
            _messageDispatcher = messageDispatcher;
        }

        /// <summary>
        /// Name of the test being executed.
        /// </summary>
        public string Name()
        {
            return(nameof(OauthTest));
        }

        /// <summary>
        /// Setup the conditions for the test.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Setup()
        {
            return(true);
        }

        /// <summary>
        /// Check a variable to see if it changes during the timeout period.
        ///
        /// This can be used to check if an event changes a variable to an expected value.
        /// </summary>
        /// <param name="variable">Variable (reference) to be examined.</param>
        /// <param name="expectedValue">Value the varaible should change to.</param>
        /// <param name="timeout">Number of seconds to allow for the change.</param>
        /// <returns>true if the variable changes to the expected within the timeout period, flase otherwise.</returns>
        private bool CheckEventVariable(ref bool variable, bool expectedValue, int timeout)
        {
            int count;
            bool checking;
            bool result = !expectedValue;

            checking = true;
            count = 0;
            while (checking)
            {
                lock (_lock)
                {
                    result = variable;
                }
                checking = (count < timeout) && (result != expectedValue);
                if (checking)
                {
                    count++;
                    Thread.Sleep(1000);
                }
            }
            return (result);
        }

        /// <summary>
        /// Execute the test synchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Execute()
        {
            return(true);
        }

        /// <summary>
        /// Execute the test asynchronously.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        /// <exception cref="NotImplementedException"></exception>
        public async Task<bool> ExecuteAsync()
        {
            bool result = false;

            try
            {
                Console.WriteLine($"Connecting to WiFi Network {Secrets.WIFI_NAME}");
                var connectionResult = await _device.WiFiAdapter.Connect(Secrets.WIFI_NAME, Secrets.WIFI_PASSWORD);
                if (connectionResult.ConnectionStatus != ConnectionStatus.Success)
                {
                    result = false;
                }
                else
                {
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
                        var content = new StringContent(authFormEncodedContent.ReadAsStringAsync().Result, Encoding.UTF8, "application/json");
                        var response = client.PostAsync(deviceAuthorizationRequestUri, content).Result;
                        response.EnsureSuccessStatusCode();
                        Console.WriteLine(response.Content.ReadAsStringAsync().Result);
                    }
                    result = true;
                }
            }
            catch
            {
                result = false;
            }

            return (result);
        }

        /// <summary>
        /// Tear down the test restoring the system to a state where other tests can be run.
        /// </summary>
        /// <returns>true if successful, false otherwise.</returns>
        public bool Teardown()
        {
            return(true);
        }
    }
}
