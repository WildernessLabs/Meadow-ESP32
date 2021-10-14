using System.Collections.Generic;
using Meadow.Devices;
using Meadow.Hardware;
using System.Text;
using System.Threading;

namespace MeadowApp
{
    /// <summary>
    /// Provide a method to communicate with the host computer over COM4.
    /// </summary>
    public class MessageDispatcher
    {
        /// <summary>
        /// Serial port connected to the host computer.
        /// </summary>
        ISerialMessagePort _serialPort;

        /// <summary>
        /// Lock used to control access to the queue.
        /// </summary>
        object _lock = new object();

        /// <summary>
        /// Queue holding the inbound messages.
        /// </summary>
        /// <typeparam name="string">Messages to be held in the queue.</typeparam>
        Queue<string> _inboundMessages = new Queue<string>();

        /// <summary>
        /// Default constructor is provate to force use of the other constructors.
        /// </summary>
        public MessageDispatcher() { }

        /// <summary>
        /// Constructor for the MessageDispatcher communications system.
        /// </summary>
        /// <param name="device">F7Micro device allowing access to the hardware.</param>
        public MessageDispatcher(F7Micro device)
        {
            _serialPort = device.CreateSerialMessagePort(device.SerialPortNames.Com4, baudRate: 115200, suffixDelimiter: Encoding.UTF8.GetBytes("\n"), preserveDelimiter: false);
            _serialPort.MessageReceived += MessageDispatcher_MessageReceived;
            _serialPort.Open();
        }

        /// <summary>
        /// New inbound message from the host computer.
        /// </summary>
        private void MessageDispatcher_MessageReceived(object sender, SerialMessageData e)
        {
            lock (_lock)
            {
                _inboundMessages.Enqueue(e.GetMessageString(Encoding.ASCII));
            }
        }

        /// <summary>
        /// Get the next message from the queue of messages.
        /// </summary>
        /// <param name="wait">Should this method wait until a message is received from the host computer.</param>
        /// <returns>Message if one is availabe, string.Empty if no messages are queued and wit is false.</returns>
        public string NextMessage(bool wait = true)
        {
            string result = string.Empty;

            bool waiting = true;
            while (waiting)
            {
                lock (_lock)
                {
                    if (_inboundMessages.Count > 0)
                    {
                        result = _inboundMessages.Dequeue();
                        waiting = false;
                    }
                    else
                    {
                        waiting = wait;
                    }
                }
                if (waiting)
                {
                    Thread.Sleep(100);
                }
            }
            return(result);
        }

        /// <summary>
        /// Report an exception to the host computer.
        /// </summary>
        /// <param name="message">Exception message to be sent to the host computer.</param>
        public void SendExceptionMessage(string message)
        {
            SendMessage($"EXCEPTION: {message}");
        }

        /// <summary>
        /// Send a message to the host computer.
        /// </summary>
        /// <param name="message">Message to send.</param>
        public void SendMessage(string message)
        {
            string output = message;
            if (!output.EndsWith("\n"))
            {
                output += "\n";
            }
            _serialPort.Write(Encoding.UTF8.GetBytes(output, 0, output.Length));
        }

        /// <summary>
        /// Send an array of strings to the host computer.
        /// </summary>
        /// <param name="messages">Strings to be sent.</param>
        public void SendMessage(string[] messages)
        {
            foreach (string message in messages)
            {
                SendMessage(message);
            }
        }
    }
}