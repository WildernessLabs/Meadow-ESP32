using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading;
using System.Diagnostics;
using System.Timers;
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
        ///     Serial port connecting the STM32 and the ESP32 microcontrollers.
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
        ///     PacketType returned by the UART Response event.
        /// </summary>
        private UARTTransportEventArgs.TransportPacketType _uartResponse = UARTTransportEventArgs.TransportPacketType.Unknown;

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
            _uart.TransportError += UartTransportError;
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
        /// <remarks>
        ///     This method simply stores the response code returned by the UART and then
        ///     signals that a response that a response has been received.  It is up to the
        ///     code queuing the items to deal with the response.
        /// </remarks>
        /// <param name="sender">Object raising the event.</param>
        /// <param name="e">Event arguments containing information about the success or failure of the send request.</param>
        private void UartTransportError(object sender, UARTTransportEventArgs e)
        {
            _uartResponse = e.PacketType;
            _waitingForResponseHandle.Set();
        }

        /// <summary>
        ///     Payload has been received from the UART so process the data.
        /// </summary>
        /// <param name="sender">Object raising the envent.</param>
        /// <param name="e">Payload received from the UART.</param>
        private void UartDataReceived(object sender, UARTDataReceivedEventArgs e)
        {
            Debug.Print($"Message received, frame type: {e.FrameType}, message ID: {e.MessageID}, sequence number: {e.SequenceNumber}, message contents:");
            DebugHelpers.DisplayBufferInHex(e.Frame);
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
                foreach (Frame frame in message)
                {
                    _uart.Send(frame.FrameType, frame.MessageID, frame.SequenceNumber, frame.Payload);
                    _waitingForResponseHandle.WaitOne();
                    if (_uartResponse != UARTTransportEventArgs.TransportPacketType.ACK)
                    {
                        //  TODO: Deal with the error here.
                    }
                }
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

        /// <summary>
        /// 
        /// </summary>
        /// <param name="packet"></param>
        /// <param name="packetLength"></param>
        private void ProcessHeader(byte[] packet, byte packetLength)
        {
            //if (packetLength != APP_RESPONSE_HEADER_LENGTH)
            //{
            //    ESP_LOGI(UART_RX_TASK_NAME, "Invalid header legth: %d", packetLength);
            //    AcknowledgeFrame(MSG_NAK, ERR_INVALID_HEADER);
            //}
            //else
            //{
            //    ResetIncomingMessage(true);
            //    messageHeader.interface = packet[0];
            //    messageHeader.function = ExtractUInt32(packet + APP_REQUEST_FUNCTION);
            //    messageHeader.messageID = ExtractUInt32(packet + APP_REQUEST_MESSAGE_ID);
            //    bytesRemaining = ExtractUInt16(packet + APP_REQUEST_PAYLOAD_LENGTH);
            //    messageHeader.payloadLength = bytesRemaining;
            //    if (messageHeader.payloadLength > 0)
            //    {
            //        messageHeader.payload = pvPortMalloc(messageHeader.payloadLength);
            //    }
            //    else
            //    {
            //        messageHeader.payload = NULL;
            //        //
            //        //  Messages with no payload should be queued immediately.
            //        //
            //        QueueMessage();
            //    }
            //    nextPayloadLocation = messageHeader.payload;
            //    AcknowledgeFrame(MSG_ACK, ERR_OK);
            //}
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="packet"></param>
        /// <param name="packetLength"></param>
        private void ProcessData(byte[] packet, byte packetLength)
        {
            //if ((nextPayloadLocation != NULL) && (bytesRemaining > 0) && (packetLength <= bytesRemaining))
            //{
            //    memcpy(nextPayloadLocation, packet, packetLength);
            //    nextPayloadLocation += packetLength;
            //    bytesRemaining -= packetLength;
            //    if (bytesRemaining == 0)
            //    {
            //        QueueMessage();
            //        //
            //        //  It is important NOT to release the memory containing the payload
            //        //  as the pointer to the payload has been copied into the queue for
            //        //  the appropriate interface.
            //        //
            //        //  It is the responsibility of the queue handler to release the payload
            //        //  memory when the handler has finished processing the message.
            //        //  Releasing the memory here will corrupt the heap.
            //        //
            //        ResetIncomingMessage(false);
            //        ESP_LOGI(UART_RX_TASK_NAME, "Message received and queued.");
            //    }
            //    AcknowledgeFrame(TRANSPORT_ACK, ERR_OK);
            //}
            //else
            //{
            //    ESP_LOGI(UART_RX_TASK_NAME, "Unexpected message data bytes (%d received, %d expected)", packetLength, bytesRemaining);
            //    AcknowledgeFrame(MSG_NAK, ERR_UNEXPECTED_DATA);
            //}
        }

        /// <summary>
        ///     A valid frame has been received, work out what to do with it.
        /// </summary>
        /// <param name="frameType">Type of frame that has been received (transport, application etc.)</param>
        /// <param name="packet">Payload to be processed.</param>
        /// <param name="packetLength">Amount of data in the payload.</param>
        private void ProcessApplicationDataFrame(byte frameType, byte[] packet, byte packetLength)
        {
            //
            //  Application data packets are either header or data packets.
            //
            //if ((frameType & DATA_HEADER) > 0)
            //{
            //    ProcessHeader(packet, packetLength);
            //}
            //else
            //{
            //    ProcessData(packet, packetLength);
            //}
        }

        #endregion Methods
    }
}
