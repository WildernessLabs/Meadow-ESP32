using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Threading.Tasks;
using Meadow;
using Meadow.Devices;
using Meadow.Hardware;
using Meadow.Foundation;
using Meadow.Foundation.Leds;
using Meadow.Gateway.WiFi;
using Meadow.Gateways;
using System.Text;
using System.Threading;

namespace MeadowApp
{
    public partial class TestRunner : App<F7Micro, TestRunner>
    {
        /// <summary>
        /// Message dispatcher implementing the message exchange between the TestRunner
        /// class and the host computer.
        /// </summary>
        MessageDispatcher _messageDispatcher;

        /// <summary>
        /// Indicate if we should output messages to the console.
        /// </summary>
        bool _verbose = false;

        /// <summary>
        /// Construct a new TestRunner object.
        /// </summary>
        public TestRunner()
        {
            Console.WriteLine("Meadow Network Test Runner");
            _messageDispatcher = new MessageDispatcher(Device);
            PopulateTestDictionary();

            _messageDispatcher.SendMessage("\n\nMeadow Network Test Runner\n");
            _messageDispatcher.SendMessage("Ready");
            Log("Waiting for commands.");
        }

        /// <summary>
        /// Output log messages to the console along with the time the message was generated.
        /// </summary>
        /// <param name="message">Message to be displayed on the console.</param>
        private void Log(string message)
        {
            if (_verbose)
            {
                string now = DateTime.Now.ToString("HH:mm:ss");
                Console.WriteLine($"[{now}] {message}");
            }
        }

        /// <summary>
        /// Execute a single named test.
        /// </summary>
        /// <param name="name">Name of the test (for messaging).</param>
        /// <param name="test">Test to be executed.</param>
        private async Task ExecuteTest(IMeadowTest test)
        {
            _messageDispatcher.SendMessage($"{test.Name()} Start");
            if (!test.Setup())
            {
                _messageDispatcher.SendMessage($"{test.Name()} FAIL: Setup");
            }
            else
            {
                bool result = false;
                if (test.AsyncTest)
                {
                    result = await test.ExecuteAsync();
                }
                else
                {
                    result = test.Execute();
                }
                if (!result)
                {
                    _messageDispatcher.SendMessage($"{test.Name()} FAIL: Execute");
                }
                else
                {
                    if (!test.Teardown())
                    {
                        _messageDispatcher.SendMessage($"{test.Name()} FAIL: Teardown");
                    }
                    else
                    {
                        _messageDispatcher.SendMessage($"{test.Name()} PASS");
                    }
                }
            }
            _messageDispatcher.SendMessage($"{test.Name()} End");
        }

        /// <summary>
        /// Find the test to be executed in the dictionary of tests.
        /// </summary>
        /// <param name="name">Name of the test to be searched for.</param>
        /// <returns>Test to be executed (if found), null otherwise.</returns>
        private IMeadowTest GetTest(string name)
        {
            IMeadowTest test = null;

            foreach (KeyValuePair<string, IMeadowTest> kvp in _tests)
            {
                if (kvp.Key.ToLower() == name.ToLower())
                {
                    test = kvp.Value;
                    break;
                }
            }
            return(test);
        }

        /// <summary>
        /// Execute one or more tests specified.
        /// </summary>
        /// <param name="names">Array of strings containing the names of the tests to be executed.</param>
        private void ExecuteTests(string[] names)
        {
            if (names.Length > 1)
            {
                for (int index = 1; index < names.Length; index++)
                {
                    IMeadowTest test = GetTest(names[index].Trim());
                    if (test != null)
                    {
                        ExecuteTest(test).Wait();
                    }
                    else
                    {
                        _messageDispatcher.SendMessage($"LOG: Cannot locate test {test}");
                        Console.WriteLine($"Cannot locate test {names[index].Trim()}");
                    }
                }
            }
            else
            {
                Console.WriteLine("ExecuteTests: No tests to execute.");
            }
        }

        /// <summary>
        /// Execute all of the tests in the dictionary of known tests.
        /// </summary>
        private void ExecuteAllTests()
        {
            foreach (KeyValuePair<string, IMeadowTest> kvp in _tests)
            {
                ExecuteTest(kvp.Value).Wait();
            }
        }

        /// <summary>
        /// Respond to the commands from the host computer.
        /// </summary>
        public void Run()
        {
            bool running = true;
            while (running)
            {
                string request = _messageDispatcher.NextMessage();

                string[] tokens = request.ToLower().Split(' ');
                switch (tokens[0].Trim())
                {
                    case "getdeviceinfo":
                        ExecuteTest(new DeviceInfoTest(Device, _messageDispatcher)).Wait();
                        break;
                    case "executetests":
                        ExecuteTests(tokens);
                        break;
                    case "executeall":
                        ExecuteAllTests();
                        break;
                    case "verbose":
                        _verbose = true;
                        break;
                    case "exit":
                        running = false;
                        break;
                    default:
                        Log($"Unknown command: {tokens[0]}");
                        break;
                }
            }
        }
    }
}
