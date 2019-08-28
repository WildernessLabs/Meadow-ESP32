/*
 *  SpiTransportProvider.cpp
 *
 *  Hardware transport provider for messages between the ESP32 and
 *  the STM32 using the SPI interface.
 */
#include "SpiTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include <string.h>

#include "esp_heap_caps.h"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/*
 *  Default state is that the class has not been fully constructed
 *  and so there is no current instance.
 */
SpiTransportProvider *SpiTransportProvider::_spiInstance = nullptr;

/*
 *  Initial state of the system is Invalid.  The constructor will set
 *  up the SPI interface and then move the state to one where we can
 *  receive data.
 */
SpiTransportProvider::SpiStates SpiTransportProvider::_currentState = Invalid;

/*
 *  Buffers will be allocated as needed and so they are initially null.
 */
uint8_t *SpiTransportProvider::_headerReceiveBuffer = nullptr;
uint8_t *SpiTransportProvider::_headerTransmitBuffer = nullptr;
uint8_t *SpiTransportProvider::_transportFrameBuffer = nullptr;
uint32_t SpiTransportProvider::_headerFrameSize = 0;

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/*
 *  Default constructor for the SPI transport provider.
 */
SpiTransportProvider::SpiTransportProvider() :
        TransportProviderBase()
{
    if (_spiInstance)
    {
        throw new MultipleInstancesException();
    }
    /*
     *  Should only get here if this is the first instance.
     */
    _spiInstance = this;

    /*
     *  Set the handshake pins to output and set the level low for
     *  the output pin and register the ISR for the input pin.
     */
    gpio_set_direction(Gpio::ESP32_READY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    /*
     *  Configure the pull-ups on the SPI pins.
     */
    gpio_set_pull_mode(Gpio::MOSI_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(Gpio::CLOCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(Gpio::CHIP_SELECT_PIN, GPIO_PULLUP_ONLY);

    /*
     *  Configure the chip select pin for the SPI bus to allow interrupts.
     */
    gpio_config_t chipSelectConfiguration;
    memset(&chipSelectConfiguration, 0, sizeof(gpio_config_t));
    chipSelectConfiguration.intr_type = GPIO_INTR_DISABLE;
    chipSelectConfiguration.mode = GPIO_MODE_INPUT;
    chipSelectConfiguration.pin_bit_mask = (1 << Gpio::CHIP_SELECT_PIN);
    gpio_config(&chipSelectConfiguration);

    /*
     *  Setup the SPI bus.
     */
    spi_bus_config_t spiBusConfiguration;
    memset(&spiBusConfiguration, 0, sizeof(spi_bus_config_t));
    spiBusConfiguration.mosi_io_num = Gpio::MOSI_PIN;
    spiBusConfiguration.miso_io_num = Gpio::MISO_PIN;
    spiBusConfiguration.sclk_io_num = Gpio::CLOCK_PIN;

    /*
     *  Configuration for the SPI slave interface.
     */
    spi_slave_interface_config_t slaveConfiguration;
    memset(&slaveConfiguration, 0, sizeof(spi_slave_interface_config_t));
    slaveConfiguration.mode = 0;
    slaveConfiguration.spics_io_num = Gpio::CHIP_SELECT_PIN;
    slaveConfiguration.queue_size = 3;
    slaveConfiguration.flags = 0;
    slaveConfiguration.post_setup_cb = SpiPostSetupCallback;
    slaveConfiguration.post_trans_cb = SpiPostTransmissionCallback;

    ESP_ERROR_CHECK(spi_slave_initialize(VSPI_HOST, &spiBusConfiguration, &slaveConfiguration, 1));

    /*
     *  The initial state is to wait for a message header to be sent by the STM32.
     *
     *  The _headerReceiveBuffer and _headerTransmitBuffer are permanently allocated
     *  as they will be frequently used for header and transport packets.
     */
    _headerFrameSize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE);
    _headerReceiveBuffer = (uint8_t *) heap_caps_malloc(_headerFrameSize, MALLOC_CAP_DMA);
    _headerTransmitBuffer = (uint8_t *) heap_caps_malloc(_headerFrameSize, MALLOC_CAP_DMA);
    ClearTransmitAndReceiveBuffers(_headerReceiveBuffer, _headerTransmitBuffer, _headerFrameSize);

    _emptyMessageHeader = (uint8_t *) pvPortMalloc(_headerFrameSize);
    memset((void *) _emptyMessageHeader, 0, _headerFrameSize);

    _currentState = WaitingForInboundMessageHeader;

    /*
     *  Set up the task and that will process the SPI messages.
     */
    xTaskCreate(Task, "SPIMessageTask", 2048 * 2, this, configMAX_PRIORITIES - 1, &SpiTransportProvider::_transportProviderTaskHandle);
}

/*
 *  Default destructor for the SPI transport provider.
 */
SpiTransportProvider::~SpiTransportProvider()
{
    spi_slave_free(VSPI_HOST);
    if (_headerReceiveBuffer)
    {
        vPortFree(_headerReceiveBuffer);
    }
    if (_headerTransmitBuffer)
    {
        vPortFree(_headerTransmitBuffer);
    }
    if (_incomingMessage)
    {
        vPortFree(_incomingMessage);
    }
    if (_outboundMessage)
    {
        vPortFree(_outboundMessage);
    }
}

/* ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/*
 *  Convert a state into a meaningful message for debugging.
 *
 *  @param state
 *      State to be decoded.
 *
 *  @returns
 *      Text representation of an SpiState.
 */
const char *SpiTransportProvider::SpiStateString(SpiTransportProvider::SpiStates state)
{
    const char *stateString = NULL;

    switch (state)
    {
        case Invalid:
            stateString = "Invalid";
            break;
        case WaitingForInboundMessageHeader:
            stateString = "WaitingForInboundMessageHeader";
            break;
        case AcknowledgingInboundMessageHeader:
            stateString = "AcknowledgingInboundMessageHeader";
            break;
        case WaitingForInboundMessageBody:
            stateString = "WaitingForMessageBody";
            break;
        case AcknowledgingMessageBody:
            stateString = "AcknowledingMessageBody";
            break;
        case SendingMessageHeader:
            stateString = "SendingMessageHeader";
            break;
        case WaitingForMessageHeaderAcknowledgement:
            stateString = "WaitingForMessageHeaderAcknowledgement";
            break;
        case SendingMessageBody:
            stateString = "SendingMessageBody";
            break;
        case WaitingForMessageBodyAcknowledgement:
            stateString = "WaitingForMessageBodyAcknowledgement";
            break;
        case WantToSendTransportMessage:
            stateString = "WantToSendTransportMessage";
            break;
        case SendingTransportResponse:
            stateString = "SendingTransportResponse";
            break;
    }
    return (stateString);
}

/*
 *  Clear the transaction receive and transmit buffers.
 *
 *  Note that it is assumed that the receive and transmit buffers are the
 *  same size.
 *
 *  @param receiveBuffer
 *      Pointer to the buffer that will hold the data that will be received.
 *
 *  @param transmitBuffer
 *      Pointer to the buffer that will hold the data to be sent.
 *
 *  @param bufferSize
 *      Number of bytes in the receive and transmit buffers.
 */
void SpiTransportProvider::ClearTransmitAndReceiveBuffers(uint8_t *transmitBuffer, uint8_t *receiveBuffer, uint32_t bufferSize)
{
    memset(transmitBuffer, 0, bufferSize);
    memset(receiveBuffer, 0, bufferSize);
}

/*
 *  Start the acknowledgement process.
 *
 *  @param message
 *      Message to acknowledge.
 *
 *  @param code
 *      Status code to return.
 *
 *  @param messageType
 *      Type of message to return.
 *
 *  @returns spi_slave_transaction_t object
 *      This is the transaction that is returned by the ESP-IDF.
 *
 *  @Notes
 *      This method allocates the transaction and two buffers on the heap.  It is essential
 *      that the caller cleans up both the transaction and the buffers after they are no
 *      longer necessary.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateAcknowledgementTransaction(Message *message, StatusCodes::StatusCodes code, uint8_t messageType)
{
    ESP_LOGI(__func__, "Entering method.");
    spi_slave_transaction_t *transaction = CreateSpiHeaderTransaction();

    Message *copyOfMessage = Message::CreateCopyOnHeap(message, false);
    copyOfMessage->MessageType = messageType;
    copyOfMessage->StatusCode = (uint32_t) code;
    uint8_t *ackFrame = (uint8_t *) Encoders::EncodeMessage(copyOfMessage, true);
    DeleteMessageAndReleaseHeapStorage(copyOfMessage);
    memcpy((void *) transaction->tx_buffer, ackFrame, _headerFrameSize);
    vPortFree(ackFrame);

    return (transaction);
}

/*
 *  Setup a new transaction using the _headerReceiveBuffer and
 *  _headerTransmitBuffer.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateSpiHeaderTransaction()
{
    ESP_LOGI(__func__, "Entering method.");
    spi_slave_transaction_t *transaction = (spi_slave_transaction_t *) pvPortMalloc(sizeof(spi_slave_transaction_t));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));
    ClearTransmitAndReceiveBuffers(_headerReceiveBuffer, _headerTransmitBuffer, _headerFrameSize);
    transaction->rx_buffer = _headerReceiveBuffer;
    transaction->tx_buffer = _headerTransmitBuffer;
    transaction->length = _headerFrameSize * 8;

    return (transaction);
}

spi_slave_transaction_t *SpiTransportProvider::CreateSpiMessageTransaction(Message *message)
{
    ESP_LOGI(__func__, "Entering method.");
    uint8_t *encodedMessage = Encoders::EncodeMessage(message, false);
    ESP_LOGI(__func__, "Want to send:");
    ESP_LOG_BUFFER_HEX(__func__, encodedMessage, Message::HEADER_SIZE + message->PayloadLength);

    uint32_t encodedMessageBufferSize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE + message->PayloadLength);
    spi_slave_transaction_t *transaction = CreateSpiPayloadTransaction(encodedMessageBufferSize);
    memcpy((void *) transaction->tx_buffer, (void *) encodedMessage, Message::HEADER_SIZE + message->PayloadLength);
    vPortFree(encodedMessage);

    return(transaction);
}

/*
 *  Create a new SPI slave transmission object with zero filled
 *  buffers of the requested size.
 *
 *  @param bufferSize
 *      Number of bytes in the receive and transmit buffers.
 *
 *  @returns
 *      Pointer to a spi_slave_transaction_t object with the
 *      buffers set up and filled with zeroes.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateSpiPayloadTransaction(uint32_t bufferSize)
{
    ESP_LOGI(__func__, "Entering method.");
    spi_slave_transaction_t *transaction = (spi_slave_transaction_t *) pvPortMalloc(sizeof(spi_slave_transaction_t));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));
    uint8_t *transmitBuffer = (uint8_t *) heap_caps_calloc(bufferSize, 1, MALLOC_CAP_DMA);
    uint8_t *receiveBuffer = (uint8_t *) heap_caps_calloc(bufferSize, 1, MALLOC_CAP_DMA);
    ClearTransmitAndReceiveBuffers(receiveBuffer, transmitBuffer, bufferSize);

    transaction->rx_buffer = receiveBuffer;
    transaction->tx_buffer = transmitBuffer;
    transaction->length = bufferSize * 8;

    return (transaction);
}

/*
 *  Release any storage allocated on the heap associated
 *  with an SPI transaction.
 *
 *  This should be used for message transactions where the Tx and Rx buffers are transient.
 *
 *  @param transaction
 *      Pointer to the transaction (on the heap)
 */
void SpiTransportProvider::DeleteMessageTransaction(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering method.");
    if (transaction)
    {
        if (transaction->rx_buffer != _headerReceiveBuffer)
        {
            heap_caps_free((void *) transaction->rx_buffer);
        }
        if (transaction->tx_buffer != _headerTransmitBuffer)
        {
            heap_caps_free((void *) transaction->tx_buffer);
        }
        vPortFree(transaction);
    }

    transaction = nullptr;
}

/*
 *  Release any storage associated with the SPI header transaction
 *
 *  This should be used for SPI transactions which use header information only
 *  i.e. ACK / NAK / transport messages.
 *
 *  This method does not release the Tx and Rx buffers as these are in constant use
 *  for header message types.
 *
 *  @param transaction
 *      Pointer to the transaction (on the heap)
 */
void SpiTransportProvider::DeleteHeaderTransaction(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering method.");
    if (transaction)
    {
        vPortFree(transaction);
    }

    transaction = nullptr;
}

/*
 *  SPI Post Transmission callback
 *
 *  This method is called when the SPI interface has received
 *  the full data load from the STM32.
 *
 *  @param transaction
 *      spi_slave_transaction_t structure containing information about the
 *      SPI transaction that has just completed.
 */
void SpiTransportProvider::SpiPostTransmissionCallback(spi_slave_transaction_t *transaction)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(_transportProviderTaskHandle, TransportProviderBase::STM32_INTERRUPT_NOTIFICATION_BIT, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR();
}

/*
 *  SPI Post setup callback
 *
 *  This method is called when the SPI interface has completed
 *  setup and data is ready for transmission.
 *
 *  @param transaction
 *      spi_slave_transaction_t structure containing information about the
 *      SPI transaction that has just completed.
 */
void SpiTransportProvider::SpiPostSetupCallback(spi_slave_transaction_t *transaction)
{
}

/*
 *  Setup the SPI transport provider.
 *
 *  @param messageDispatcher
 *      MessageDispatcher object used to send messages from the SPI
 *      interface through to the interfaces on the ESP32.
 */
void SpiTransportProvider::Setup(MessageDispatcher *messageDispatcher)
{
    TransportProviderBase::Setup(messageDispatcher);
}

/*
 *  The Echo function simply sends a message through the various communication layers.
 *  This is a test function to verify correct communication through the message
 *  stack.
 *
 *  @param messageString
 *      Message to send through the communication layers.
 */
void SpiTransportProvider::Echo(const char *messageString)
{
    Message *message;

    message = (Message *) pvPortMalloc(sizeof(Message));
    message->Interface = Esp32Interfaces::System;
    message->Function = SystemFunction::Echo;
    message->MessageID = 0x00;
    message->MessageType = MessageTypes::Data;
    message->PayloadLength = strlen(messageString) + 1;
    char *str = (char *) pvPortMalloc(message->PayloadLength);
    strcpy(str, messageString);
    message->Payload = (uint8_t *) str;
    _messageDispatcher->QueueMessageForEsp32(message);
}

/*
 *  Clear the class level variable _currentHeader.
 *
 *  This method ensures that any heap storage is released and the
 *  pointer is set to the nullptr value.
 *
 *  @param message
 *      Pointer to the message to be cleared.
 */
void SpiTransportProvider::DeleteMessageAndReleaseHeapStorage(Message *message)
{
    ESP_LOGI(__func__, "Entering Method.");

    if (message)
    {
        if (message->Payload)
        {
            vPortFree(message->Payload);
            message->Payload = nullptr;
        }
        vPortFree(message);
    }
    heap_caps_check_integrity_all(true);
}

/*
 *  Encode a header or transport only message.
 *
 *  @param transaction
 *      SPI transaction that has been created to hold the message/header.
 *
 *  @param message
 *      Message or header to be encoded and copied into the SPI transaction
 *      Tx buffer.
 */
void SpiTransportProvider::EncodeHeaderMessage(spi_slave_transaction_t *transaction, Message *message)
{
    ESP_LOGI(__func__, "Entering Method.");

    uint8_t *encodedMessageHeader = Encoders::EncodeMessage(message, true);
    memcpy((void *) transaction->tx_buffer, (void *) encodedMessageHeader, _headerFrameSize);
    vPortFree(encodedMessageHeader);
}

/*
 *  Process the transport message and prepare the next transaction and
 *  change to the appropriate state.
 *
 *  @return
 *      Pointer to a new transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::ProcessTransportMessage()
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t *newTransaction = nullptr;
    switch (_incomingMessage->Function)
    {
        case TransportFunction::SendResponse:
            _messageType = MessageTypes::Transport;
            if (OutboundMessageQueueLength() > 0)
            {
                ESP_LOGI(__func__, "Outbound messages waiting.");
                _responseCode = StatusCodes::CompletedOK;
            }
            else
            {
                ESP_LOGI(__func__, "No outbound messages queued.");
                _responseCode = StatusCodes::NoMessagesWaiting;
            }
            Message *message;
            message = Message::CreateOnHeap(MessageTypes::Transport, Esp32Interfaces::Transport, TransportFunction::SendResponse, _responseCode);
            newTransaction = CreateSpiHeaderTransaction();
            EncodeHeaderMessage(newTransaction, message);
            vPortFree((void *) message);
            _currentState = SendingTransportResponse;
            break;
        default:
            ESP_LOGI(__func__, "Transport function %d not recognised.", _incomingMessage->Function);
            _responseCode = StatusCodes::CompletedOK;
            _messageType = MessageTypes::Nak;
            break;
    }

    DeleteMessageAndReleaseHeapStorage(_incomingMessage);
    _incomingMessage = nullptr;
    return(newTransaction);
}

/*
 *  The STM32 has sent what is assumed to be a message header to the ESP32
 *
 *  @param transaction
 *      Current SPI transaction
 *
 *  @returns
 *      New SPI transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::InboundMessageHeaderReceived(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);

    DeleteMessageAndReleaseHeapStorage(_incomingMessage);
    _incomingMessage = nullptr;
    spi_slave_transaction_t *newTransaction = nullptr;
    _currentState = AcknowledgingInboundMessageHeader;
    if (dataSize >= _headerFrameSize)
    {
        _incomingMessage = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, _headerFrameSize, true);
        if (!_incomingMessage)             // nullptr indicates a CRC error.
        {
            ESP_LOGI(__func__, "CRC Error");
            _responseCode = StatusCodes::CRCError;
            _messageType = MessageTypes::Nak;
            _incomingMessage = Message::CreateOnHeap(_messageType, Esp32Interfaces::Transport, 0, _responseCode);
        }
        else
        {
            switch (_incomingMessage->MessageType)
            {
                case MessageTypes::Header:
                    _messageType = MessageTypes::Ack;
                    _responseCode = StatusCodes::CompletedOK;
                    newTransaction = CreateAcknowledgementTransaction(_incomingMessage, _responseCode, _messageType);
                    break;
                case MessageTypes::Transport:
                    newTransaction = ProcessTransportMessage();
                    break;
                default:
                    ESP_LOGI(__func__, "Unexpected message type: %d", _incomingMessage->MessageType);
                    _responseCode = StatusCodes::UnexpectedData;
                    _messageType = MessageTypes::Nak;
                    DeleteMessageAndReleaseHeapStorage(_incomingMessage);
                    _incomingMessage = Message::CreateOnHeap(_messageType, Esp32Interfaces::Transport, 0, _responseCode);
                    newTransaction = CreateAcknowledgementTransaction(_incomingMessage, _responseCode, _messageType);
                    break;
            }
        }
    }
    else
    {
        ESP_LOGI(__func__, "Too little data, received %d bytes, expected %d bytes", dataSize, _headerFrameSize);
        _responseCode = StatusCodes::InvalidHeader;
        _messageType = MessageTypes::Nak;
        _incomingMessage = Message::CreateOnHeap(_messageType, Esp32Interfaces::Transport, 0, _responseCode);
        newTransaction = CreateAcknowledgementTransaction(_incomingMessage, _responseCode, _messageType);
    }
    DeleteHeaderTransaction(transaction);

    if (_currentState != WaitingForInboundMessageHeader)
    {
        ESP_LOGI(__func__, "Preparing response, packet type: %d, status code: %d", _messageType, _responseCode);
    }
    return(newTransaction);
}

/*
 *  Acknowledge the message header that has been received and work out the next steps.
 *
 *  If the header is valid then the response will be an acknowledgement and
 *      - Messages with no payload should be queued
 *      - Messages with payloads should enter the waiting for message body state.
 *
 *  If the header was corrupt then there is little we can do other than send a NAK
 *  message and then throw away the data and wait for the sender to restart the
 *  sequence.
 *
 *  Upon entry it is essential that the following variables have been set up:
 *      _incomingMessage
 *      _responseCode
 *      _messageType
 *
 *  @params transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      New SPI transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::InboundMessageHeaderAcknowledged(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t *newTransaction = nullptr;
    if (_responseCode == StatusCodes::CompletedOK)
    {
        if (_incomingMessage->PayloadLength == 0)         // Assume _incomingMessage has been set up.
        {
            //
            //  If we get here then the message is simple, i.e. no payload
            //  so we need to queue the message on the appropriate interface.
            //
            if (_incomingMessage->Interface != Esp32Interfaces::Transport)
            {
                _messageDispatcher->QueueMessageForEsp32(_incomingMessage);
                _incomingMessage = nullptr;
            }
            newTransaction = CreateSpiHeaderTransaction();
            _currentState = WaitingForInboundMessageHeader;
            _incomingMessage = nullptr;
        }
        else
        {
            uint32_t encodedMessageBodySize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE + _incomingMessage->PayloadLength);
            newTransaction = CreateSpiPayloadTransaction(encodedMessageBodySize);

            ESP_LOGI(__func__, "Current header payload length: %d", _incomingMessage->PayloadLength);
            ESP_LOGI(__func__, "Message body buffer size: %d", encodedMessageBodySize);
            _currentState = WaitingForInboundMessageBody;
        }
    }
    else
    {
        DeleteMessageTransaction(_spiTransaction);
        DeleteMessageAndReleaseHeapStorage(_incomingMessage);
        _incomingMessage = nullptr;
        newTransaction = CreateSpiHeaderTransaction();
        _currentState = WaitingForInboundMessageHeader;
    }
    return(newTransaction);
}

/*
 *  Previous messages have indicated that a message body is required, request the message body.
 *
 *  This method will set up the response types but will leave the _currentHeader variable in place.
 *  Memory allocated to _currentHeader must be deallocated in the Acknowledgement method.
 *
 *  @params transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      New SPI transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::InboundMessageBodyReceived(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
    uint32_t encodedMessageBodySize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE + _incomingMessage->PayloadLength);
    //
    //  Free up the SPI Tx buffer as soon as we can when receiving message bodies as the
    //  contents of the buffer should be 0 and are meaningless.  This is done to mitigate
    //  against running low / out of memory on the heap.
    //
    heap_caps_free((void *) transaction->tx_buffer);
    transaction->tx_buffer = nullptr;
    Message *message = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, encodedMessageBodySize, false);

    uint8_t messageType = MessageTypes::Ack;
    StatusCodes::StatusCodes responseCode = StatusCodes::CompletedOK;
    if (message)
    {
        if (dataSize != encodedMessageBodySize)
        {
            responseCode = StatusCodes::InvalidPacket;
            messageType = MessageTypes::Nak;
        }
        else
        {
            if (message->MessageType == MessageTypes::Data)
            {
                if ((message->Interface == _incomingMessage->Interface) && (message->Function == _incomingMessage->Function) && (message->MessageID == _incomingMessage->MessageID))
                {
                    _messageDispatcher->QueueMessageForEsp32(message);
                }
                else
                {
                    responseCode = StatusCodes::UnexpectedData;
                    messageType = MessageTypes::Nak;
                }
            }
            else
            {
                responseCode = StatusCodes::UnexpectedData;
                messageType = MessageTypes::Nak;
            }
        }
    }
    else
    {
        responseCode = StatusCodes::CRCError;
        messageType = MessageTypes::Nak;
    }
    DeleteMessageTransaction(transaction);
    _responseCode = responseCode;
    _messageType = messageType;

    ESP_LOGI(__func__, "Preparing response, packet type: %d, status code: %d", messageType, responseCode);
    spi_slave_transaction_t *newTransaction = CreateAcknowledgementTransaction(_incomingMessage, responseCode, messageType);

    DeleteMessageAndReleaseHeapStorage(_incomingMessage);
    _incomingMessage = nullptr;

    _currentState = AcknowledgingMessageBody;
    return(newTransaction);
}

/*
 *  Acknowledge the receipt of the message body.
 *
 *  This method requires the following class level variables to have
 *  been setup correctly:
 *      _responseCode
 *
 *  @params transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      New SPI transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::InboundMessageBodyAcknowledged(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    DeleteHeaderTransaction(transaction);
    spi_slave_transaction_t *newTransaction = CreateSpiHeaderTransaction();
    _currentState = WaitingForInboundMessageHeader;
    return (newTransaction);
}

/*
 *  The message header will already have been sent, we need to get ready to
 *  receive and ACK or NAK from the STM32.
 *
 *  @params transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      SPI transaction created to receive the message acknowledgement.
 */
spi_slave_transaction_t *SpiTransportProvider::OutboundMessageHeaderSent(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    DeleteHeaderTransaction(transaction);
    spi_slave_transaction_t *newTransaction = CreateSpiHeaderTransaction();
    if (_outboundMessage)
    {
        _currentState = WaitingForMessageHeaderAcknowledgement;
    }
    else
    {
        ESP_LOGE(__func__, "No message to send.");
        _currentState = WaitingForInboundMessageHeader;
    }
    return(newTransaction);
}

/*
 *  Implement processing of the incoming message header acknowledgement messages.
 *
 *  This function can result in the ESP32 moving in one of three ways:
 *      - STM32 received a corrupt header and so the ESP32 should add the
 *        message back on the outbound queue.
 *      - The message header was received OK and there is a body to send.
 *        A new transaction is created to send the message body.
 *      - The message header was received OK but there is no body in the
 *        message.  In this case we move to receive another header from the
 *        STM32.
 *
 *  @param transaction
 *      SPI transaction that was used to get the message header acknowledgement.
 *
 *  @return
 *      New SPI transaction that can be used to either communicate with the STM32.
 *
 */
spi_slave_transaction_t *SpiTransportProvider::OutboundMessageHeaderAcknowledgementReceived(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    //
    //  First extract the header so that we can work out what action should be taken.
    //
    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
    Message *acknowledgement = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, _headerFrameSize, true);

    DeleteHeaderTransaction(transaction);
    spi_slave_transaction_t *newTransaction = nullptr;

    if (acknowledgement)
    {
        if (acknowledgement->MessageType == MessageTypes::Ack)
        {
            ESP_LOGI(__func__, "Acknowledgement received.");
            if (_outboundMessage->PayloadLength > 0)
            {
                ESP_LOGI(__func__, "Creating payload message.");
                newTransaction = CreateSpiMessageTransaction(_outboundMessage);
                _currentState = SendingMessageBody;
            }
            else
            {
                ESP_LOGI(__func__, "No payload to send.");
                DeleteMessageAndReleaseHeapStorage(_outboundMessage);
                newTransaction = CreateSpiHeaderTransaction();
                _currentState = WaitingForInboundMessageHeader;
            }
        }
        else
        {
            ESP_LOGI(__func__, "NAK received.");
            QueueMessageForStm32(_outboundMessage);
            newTransaction = CreateSpiHeaderTransaction();
            _currentState = WaitingForInboundMessageHeader;
        }
        vPortFree(acknowledgement);
    }
    else
    {
        ESP_LOGI(__func__, "No acknowledgement received.");
        QueueMessageForStm32(_outboundMessage);
        newTransaction = CreateSpiHeaderTransaction();
        _currentState = WaitingForInboundMessageHeader;
    }
    _outboundMessage = nullptr;
    ESP_LOGI(__func__, "Exiting.");
    return(newTransaction);
}

/*
 *  The message body has been sent, we should now prepare to receive the message
 *  body acknowledgement.
 *
 *  The message acknowledgement (ACK / NAK) will determine if the STM32 has received
 *  the message body.
 *
 *  @param transaction
 *      Last SPI transaction that was executed.
 *
 *  @return
 *      New SPI transaction with data.
 */
spi_slave_transaction_t *SpiTransportProvider::OutboundMessageBodySent(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    DeleteMessageTransaction(transaction);
    spi_slave_transaction_t *newTransaction = CreateSpiHeaderTransaction();
    _currentState = WaitingForMessageBodyAcknowledgement;

    return(newTransaction);
}

/*
 *  Implement processing of the out-bound message body acknowledgement.
 *
 *  The message body should have been sent and the STM32 will have responded
 *  with an ACK or NAK.  If the message has failed (i.e. corrupt) then the
 *  ESP should just put the message back on the queue.
 *
 *  If the body was received then free up any memory used and prepare to
 *  receive the next message header.
 *
 *  @param transaction
 *      Last SPI transaction that was executed.
 *
 *  @return
 *      New SPI transaction with data.
 */
spi_slave_transaction_t *SpiTransportProvider::OutboundMessageBodyAcknowledgementReceived(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
    Message *incomingMessage = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, _headerFrameSize, true);
    DeleteMessageTransaction(transaction);

    if (incomingMessage)
    {
        if (incomingMessage->MessageType == MessageTypes::Ack)
        {
            ESP_LOGI(__func__, "Acknowledgement received.");
            DeleteMessageAndReleaseHeapStorage(_outboundMessage);
        }
        else
        {
            QueueMessageForStm32(_outboundMessage);
        }
        vPortFree(incomingMessage);
    }
    else
    {
        QueueMessageForStm32(_outboundMessage);
    }
    _currentState = WaitingForInboundMessageHeader;
    _outboundMessage = nullptr;

    return(CreateSpiHeaderTransaction());
}

/*
 *  Check to see if there are any messages waiting to be sent and start the
 *  process of sending the message if there are any queued.
 *
 *  When this function exists, the transaction returned will be one of:
 *  - The message header of the first message waiting to be sent and
 *    the state will be set to indicate that we want to send this.
 *  - An empty message header and we will return to the idle state ready
 *    to receive messages from the STM32.
 *
 *  @param transaction
 *      Last SPI transaction that was executed.
 *
 *  @return
 *      New SPI transaction with data.
 */
spi_slave_transaction_t *SpiTransportProvider::ProcessSendingTransportResponseState(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    DeleteHeaderTransaction(transaction);
    spi_slave_transaction_t *newTransaction = CreateSpiHeaderTransaction();
    DeleteMessageAndReleaseHeapStorage(_outboundMessage);
    _outboundMessage = nullptr;
    if (_responseCode == StatusCodes::CompletedOK)
    {
        if (OutboundMessageQueueLength() > 0)
        {
            _outboundMessage = GetOutboundMessage();
            if (_outboundMessage)
            {
                ESP_LOGI(__func__, "Encoding response header.");
                EncodeHeaderMessage(newTransaction, _outboundMessage);
                _currentState = SendingMessageHeader;
            }
            else
            {
                ESP_LOGI(__func__, "Failed to retrieve message from queue.");
            }
        }
        else
        {
            _currentState = WaitingForInboundMessageHeader;
        }
    }
    else
    {
        _currentState = WaitingForInboundMessageHeader;
    }
    return(newTransaction);
}

/*
 *  Check to see if the message buffer contains any data.
 *
 *  @Note
 *      This method simply scans a buffer that should contain an encoded message
 *      to see if it contains any non-zero values.  This method is necessary
 *      because of a fault that occasionally occurs where the interrupt that
 *      indicates that the ESP32 has received data fires when it should not as
 *      there is no message.
 *
 *      A message cannot be all zeros as there must be non-zero bytes in the CRC.
 *
 *  @param buffer
 *      Pointer to a buffer of data.
 *
 *  @param bufferLength
 *      Number of bytes in the buffer.
 *
 *  @return
 *      true if the message contains valid (i.e. non-zero bytes) data.
 *      false if the buffer is not a valid encoded message.
 */
bool SpiTransportProvider::MessageContainsData(uint8_t *buffer, uint32_t bufferLength)
{
    bool result = false;
    for (uint32_t index = 0; index < bufferLength; index++)
    {
        if (buffer[index] != 0)
        {
            result = true;
            break;
        }
    }
    return(result);
}

/*
 *  Check to see if the ESP32 is sending data.
 *
 *  @param state
 *      State to check.
 *
 *  @return
 *      true if the state indicates that the ESP32 is sending data,
 *      false otherwise.
 */
bool SpiTransportProvider::SendingData(SpiStates state)
{
    return((_currentState == WaitingForInboundMessageHeader)
            || (_currentState == AcknowledgingInboundMessageHeader)
            || (_currentState == AcknowledgingMessageBody)
            || (_currentState == SendingMessageHeader)
            || (_currentState == SendingMessageBody)
            || (_currentState == SendingTransportResponse));
}

/*
 *  Start the SPI communication and with for a signal that we have messages to process.
 *  Process the incoming message and return a new transaction that can be used for the
 *  next stage in the process.
 *
 *  @param transaction
 *      Valid SPI transaction that can be used to communicate with the STM32.
 *
 *  @return
 *      Newly setup SPI transaction that can be used in the next stage of the process.
 *
 */
spi_slave_transaction_t *SpiTransportProvider::ProcessMessages(spi_slave_transaction_t *transaction)
{
    spi_slave_transaction_t *newTransaction = nullptr;

    if ((_currentState == WaitingForInboundMessageHeader) && (OutboundMessageQueueLength() > 0))
    {
        gpio_set_level(Gpio::ESP32_READY_PIN, 1);
    }
    else
    {
        gpio_set_level(Gpio::ESP32_READY_PIN, 0);
    }

    uint32_t notificationBits = 0;
    ESP_LOGI(__func__, "Waiting for notification, current state: %s", SpiStateString(_currentState));
    ESP_ERROR_CHECK(spi_slave_queue_trans(VSPI_HOST, transaction, portMAX_DELAY));
    xTaskNotifyWait(0x00000000, ULONG_MAX, &notificationBits, portMAX_DELAY);
    //
    //  ESP-IDF documentation states that a call to spi_slave_get_trans_result
    //  is mandatory when spi_slave_queue_trans is used.
    //
    ESP_ERROR_CHECK(spi_slave_get_trans_result(VSPI_HOST, &transaction, portMAX_DELAY));
    if (SendingData(_currentState) || MessageContainsData((uint8_t *) transaction->rx_buffer, transaction->length / 8))
    {
        switch (_currentState)
        {
            case WaitingForInboundMessageHeader:
                newTransaction = InboundMessageHeaderReceived(transaction);
                break;
            case AcknowledgingInboundMessageHeader:
                newTransaction = InboundMessageHeaderAcknowledged(transaction);
                break;
            case WaitingForInboundMessageBody:
                newTransaction = InboundMessageBodyReceived(transaction);
                break;
            case AcknowledgingMessageBody:
                newTransaction = InboundMessageBodyAcknowledged(transaction);
                break;

            case SendingMessageHeader:
                newTransaction = OutboundMessageHeaderSent(transaction);
                break;
            case WaitingForMessageHeaderAcknowledgement:
                newTransaction = OutboundMessageHeaderAcknowledgementReceived(transaction);
                break;
            case SendingMessageBody:
                newTransaction = OutboundMessageBodySent(transaction);
                break;
            case WaitingForMessageBodyAcknowledgement:
                newTransaction = OutboundMessageBodyAcknowledgementReceived(transaction);
                break;

            case SendingTransportResponse:
                newTransaction = ProcessSendingTransportResponseState(transaction);
                break;
            default:
                ESP_LOGI(__func__, "Un-handled state: %s", SpiStateString(_currentState));
                break;
        }
    }
    else
    {
        ESP_LOGI(__func__, "Empty buffer, discarding data and staying in state %s", SpiStateString(_currentState));
        newTransaction = transaction;
    }
    return(newTransaction);
}

/*
 *  Process any messages for the controller.
 *
 *  The messages will be generated by the incoming SPI interface.
 *
 *  Note that this method is static in order to allow the C methods in
 *  FreeRTOS to call the method.
 *
 *  @param pvParameters
 *      Pointer to the instance of the request handler receiving the
 *      messages for dispatch through the instance specific handler.
 */
void SpiTransportProvider::Task(void *pvParameters)
{
    SpiTransportProvider *sender = (SpiTransportProvider *) pvParameters;

    //
    //  It is important to wait for the outbound message queue to be
    //  created before we try to process any messages.
    //
    while (!_outboundMessageQueueHandle)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    spi_slave_transaction_t *transaction = sender->CreateSpiHeaderTransaction();
    while (true)
    {
        transaction = sender->ProcessMessages(transaction);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(sender->_transportProviderTaskHandle);
}
