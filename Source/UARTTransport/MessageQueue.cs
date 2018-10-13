using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using UARTTransport;

namespace Esp32Messaging
{
    public class MessageQueue
    {
        #region Private variables.

        /// <summary>
        ///     Queue of messages to be sent to the ESP32.
        /// </summary>
        private static Queue<Message> _messageQueue = null;

        /// <summary>
        ///     Lock for the message queue to synchronise access by the
        ///     two threads (main application adding messages and the
        ///     queue processing thread taking messages from the queue).
        /// </summary>
        private object _messageQueueLock = new object();

        /// <summary>
        ///     Handle used to synchronise the incoming messages and the task processing the messages.
        /// </summary>
        private EventWaitHandle _waitingForMessageHandle = new AutoResetEvent(false);

        /// <summary>
        ///     Handle used to synchronise the responses from the UART.
        /// </summary>
        private EventWaitHandle _waitingForResponseHandle = new AutoResetEvent(false);

        /// <summary>
        ///     Thread processing the messages as they enter the queue.
        /// </summary>
        private Thread _queueHandler = null;

        /// <summary>
        /// 
        /// </summary>
        private readonly UART _uart;

        /// <summary>
        ///     The message that is being received from the ESP32.
        /// </summary>
        /// <remarks>
        ///     Access to this message is not protected by a lock as the only time this variable
        ///     should be accessed is in the DateReceived event.
        ///
        ///     If this should change then locking should be considered.
        /// </remarks>
        private Message _incomingMessage = null;

        /// <summary>
        ///     Error returned by the UART Response event.
        /// </summary>
        private UARTResponseEventArgs.ErrorType _uartResponse = UARTResponseEventArgs.ErrorType.Unknown;

        #endregion Private variables

        #region Constructors

        /// <summary>
        ///     Default constructor is private to prevent it being used.
        /// </summary>
        private MessageQueue()
        {
        }

        /// <summary>
        ///     Create a new MessageQueue object and attach it to the specified COM port.
        /// </summary>
        /// <param name="comPort">Name of the com port to attach to this queue.</param>
        /// <param name="maximumRetryCount">Maximum number of serial retries before an error is generated.</param>
        public MessageQueue(string comPort, uint maximumRetryCount = 3)
        {
            if (_messageQueue != null)
            {
                throw new InvalidOperationException("Only a single instance of a MessageQueue can be created.");
            }
            _uart = new UART(comPort, maximumRetryCount);
            _uart.ResponseReceived += _uart_ResponseReceived;
            _uart.DataReceived += UartDataReceived;
            _messageQueue = new Queue<Message>();
            _queueHandler = new Thread(() =>
            {
                while (true)
                {
                    _waitingForMessageHandle.WaitOne();
                    ProcessMessages();
                }
            });
            _queueHandler.Start();
        }

        #endregion Constructors

        #region Event handlers

        /// <summary>
        ///     UART has sent a response to the request to send some data to the ESP32.
        /// </summary>
        /// <param name="sender">Object raising the event.</param>
        /// <param name="e">Event arguments containing information about the success or failure of the send request.</param>
        private void _uart_ResponseReceived(object sender, UARTResponseEventArgs e)
        {
            throw new NotImplementedException();
        }

        /// <summary>
        ///     Data has been received from the UART so process the data.
        /// </summary>
        /// <param name="sender">Object raising the envent.</param>
        /// <param name="e">Data received from the UART.</param>
        private void UartDataReceived(object sender, UARTDataReceivedEventArgs e)
        {
            throw new NotImplementedException();
        }

        #endregion Event handlers

        #region Methods

        /// <summary>
        ///     retrieve a single message from the queue.
        /// </summary>
        /// <returns>Valid message from the front of the queue if the queue is not empty, null otherwise.</returns>
        private Message GetMessageFromQueue()
        {
            Message message = null;

            lock (_messageQueueLock)
            {
                if (_messageQueue.Count > 0)
                {
                    message = _messageQueue.Dequeue();
                }
            }

            return (message);
        }

        /// <summary>
        ///     Process any waiting messages.
        /// </summary>
        /// <remarks>
        ///     This method should only really be called from within the _queueHandler thread.
        /// </remarks>
        private void ProcessMessages()
        {
            Message message = GetMessageFromQueue();
            while (message != null)
            {
                
                message = GetMessageFromQueue();
            }
        }

        /// <summary>
        ///     Add a new message to the message queue.
        /// </summary>
        /// <param name="message">Message to add to the queue.</param>
        [MethodImpl(MethodImplOptions.Synchronized)]
        public void Add(Message message)
        {
            lock (_messageQueueLock)
            {
                _messageQueue.Enqueue(message);
            }
            //
            //  Inform the task processing the messages that a new message
            //  has been queued so there is at least one message waiting to
            //  be sent.
            //
            _waitingForMessageHandle.Set();
        }

        #endregion Methods
    }
}
