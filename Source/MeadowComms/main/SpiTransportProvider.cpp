/*
 *  SpiTransportProvider.cpp
 *
 *  Hardware transport provider for messages between the ESP32 and
 *  the STM32 using the SPI interface.
 */
#include "../build/config/sdkconfig.h"

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

/**
 *  @brief Name of this component / task.
 */
const char *SpiTransportProvider::COMPONENT_NAME = "SPITask";

/**
 *  @brief Default state is that the class has not been fully constructed
 *  and so there is no current instance.
 */
SpiTransportProvider *SpiTransportProvider::_spiInstance = nullptr;

/**
 *  @brief Initial state of the system is Invalid.  The constructor will set
 *  up the SPI interface and then move the state to one where we can
 *  receive data.
 */
SpiTransportProvider::SpiStates SpiTransportProvider::_currentState = Invalid;

TaskHandle_t SpiTransportProvider::_synchronisationTaskHandle = NULL;

/**
 *  Buffers will be allocated as needed and so they are initially null.
 */
uint8_t *SpiTransportProvider::_rxBuffer = nullptr;
uint8_t *SpiTransportProvider::_txBuffer = nullptr;
uint8_t *SpiTransportProvider::_transportFrameBuffer = nullptr;
uint32_t SpiTransportProvider::_headerFrameSize = 0;

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the SPI transport provider.
 */
SpiTransportProvider::SpiTransportProvider() : TransportProviderBase()
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
     *  Set the handshake pins to output and set the output level low.
     *
     *  First pin is the pin indicating that the ESP32 SPI interface is ready
     *  to send / receive data.  This is the BOOT pin on the ESP32.
     *
     *  The second pin is the pin used by the ESP32 to indicate that a message
     *  is waiting.  This should be the UART0 Rx pin.
     */
    gpio_pad_select_gpio(Gpio::ESP32_SPI_READY_PIN);
    gpio_set_direction(Gpio::ESP32_SPI_READY_PIN, GPIO_MODE_OUTPUT);
    //
    gpio_pad_select_gpio(Gpio::ESP32_MESSAGE_WAITING_PIN);
    gpio_set_direction(Gpio::ESP32_MESSAGE_WAITING_PIN, GPIO_MODE_OUTPUT);

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
    chipSelectConfiguration.intr_type = GPIO_INTR_DISABLE;
    chipSelectConfiguration.mode = GPIO_MODE_INPUT;
    chipSelectConfiguration.pull_down_en = GPIO_PULLDOWN_DISABLE;
    chipSelectConfiguration.pull_up_en = GPIO_PULLUP_DISABLE;
    chipSelectConfiguration.pin_bit_mask = (1 << Gpio::CHIP_SELECT_PIN);
    gpio_config(&chipSelectConfiguration);

    /*
     *  Setup the SPI bus.
     */
    spi_bus_config_t spiBusConfiguration;
    spiBusConfiguration.mosi_io_num = Gpio::MOSI_PIN;
    spiBusConfiguration.miso_io_num = Gpio::MISO_PIN;
    spiBusConfiguration.sclk_io_num = Gpio::CLOCK_PIN;
    spiBusConfiguration.quadhd_io_num = -1;
    spiBusConfiguration.quadwp_io_num = -1;
    spiBusConfiguration.max_transfer_sz = 0;
    spiBusConfiguration.flags = SPICOMMON_BUSFLAG_SLAVE;
    spiBusConfiguration.intr_flags = 0;
    /*
     *  Configuration for the SPI slave interface.
     */
    spi_slave_interface_config_t slaveConfiguration;
    slaveConfiguration.mode = 3;
    slaveConfiguration.spics_io_num = Gpio::CHIP_SELECT_PIN;
    slaveConfiguration.queue_size = 3;
    slaveConfiguration.flags = 0;
    slaveConfiguration.post_trans_cb = SpiPostTransmissionCallback;
    slaveConfiguration.post_setup_cb = SpiPostSetupCallback;

    ESP_ERROR_CHECK(spi_slave_initialize(VSPI_HOST, &spiBusConfiguration, &slaveConfiguration, 1));

    /*
     *  The initial state is to wait for a message header to be sent by the STM32.
     *
     *  The _rxBuffer and _txBuffer are permanently allocated
     *  as they will be frequently used for header and transport packets.
     */
    _headerFrameSize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE);
    _txBuffer = static_cast<uint8_t *>(heap_caps_calloc(MAXIMUM_SPI_FRAME_SIZE, 1, MALLOC_CAP_DMA));
    _rxBuffer = static_cast<uint8_t *>(heap_caps_calloc(MAXIMUM_SPI_FRAME_SIZE, 1, MALLOC_CAP_DMA));
    SetMaximumMessageSize(MAXIMUM_SPI_FRAME_SIZE - _headerFrameSize);
    ClearTransmitAndReceiveBuffers();

    /*
     *  Set the state machine to be ready to receive messages.
     */
    _currentState = WaitingForMessage;

    /*
     *  Set up the task and that will process the SPI messages.
     */
    _synchronisationSemaphore = xSemaphoreCreateBinary();
    //
    //  FreeRTOS documentation states that a binary semaphore must be given after creation as it is created "empty".
    //  For more information, see https://www.freertos.org/xSemaphoreCreateBinary.html
    //
    xSemaphoreGive(_synchronisationSemaphore);
    if (xSemaphoreTake(_synchronisationSemaphore, pdMS_TO_TICKS(500)) != pdTRUE)
    {
        ERROR_MESSAGE("Cannot take the synchronisation semaphore.");
    }
    xTaskCreate(SynchronisationTask, "SPISyncTask", 2048 * 2, this, configMAX_PRIORITIES - 1, &SpiTransportProvider::_synchronisationTaskHandle);

    /*
     *  Set up the task and that will process the SPI messages.
     */
    xTaskCreate(Task, COMPONENT_NAME, 2048 * 2, this, configMAX_PRIORITIES - 1, &SpiTransportProvider::_transportProviderTaskHandle);

    /*
     *  The SPI code on the STM32 will wait for both SPI ready and
     *  message waiting to both go low as a signal that the SPI code
     *  has completed the initialisation process.
     */
    gpio_set_level(Gpio::ESP32_MESSAGE_WAITING_PIN, 0);
    gpio_set_level(Gpio::ESP32_SPI_READY_PIN, 0);

}

/**
 *  @brief Default destructor for the SPI transport provider.
 */
SpiTransportProvider::~SpiTransportProvider()
{
    spi_slave_free(VSPI_HOST);
    if (_rxBuffer)
    {
        vPortFree(_rxBuffer);
    }
    if (_txBuffer)
    {
        vPortFree(_txBuffer);
    }
    if (_incomingMessage)
    {
        Message::DeleteMessage(_incomingMessage);
    }
    if (_outboundMessage)
    {
        Message::DeleteMessage(_outboundMessage);
    }
    _currentState = Invalid;
}

/*
 * ----------------------------------------------------------------------------
 *
 *                      Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Get the maximum size of a message that can be sent through the transport provider.
 *
 *  @returns
 *      Maximum size of a message that can be sent using the current transport provider.
 */
uint32_t SpiTransportProvider::GetMaximumMessageSize() const
{
    return(_maximumMessageSize);
}

/**
 *  Set the maximum message size that can be sent through the transport provider.
 *
 *  @param messageSize
 *      Desired maximum message size.
 *
 *  @returns
 *      true if the message size is legal, false if the message size could not be set.
 */
bool SpiTransportProvider::SetMaximumMessageSize(uint32_t maximumMessageSize)
{
    uint32_t maximumSize = MAXIMUM_SPI_FRAME_SIZE - _headerFrameSize;
    bool result = false;

    if ((maximumMessageSize <= maximumSize) && (maximumMessageSize >= _headerFrameSize))
    {
        _maximumMessageSize = maximumMessageSize;
        result = true;
    }
    return(result);
}

/* ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Convert a state into a meaningful message for debugging.
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
        case WaitingForMessage:
            stateString = "WaitingFoMessage";
            break;
        case AcknowledgingMessage:
            stateString = "AcknowledgingMessage";
            break;
        case SendingMessageDetails:
            stateString = "SendingMessageDetails";
            break;
        case SendingMessage:
            stateString = "SendingMessage";
            break;
        case WaitingForMessageAcknowledgement:
            stateString = "WaitingForMessageAcknowledgement";
            break;
    }
    return (stateString);
}

/**
 *  @brief Clear the transaction receive and transmit buffers.
 *
 *  Note that it is assumed that the receive and transmit buffers are the
 *  same size.
 *
 */
void SpiTransportProvider::ClearTransmitAndReceiveBuffers()
{
    memset(_rxBuffer, 0, MAXIMUM_SPI_FRAME_SIZE);
    memset(_txBuffer, 0, MAXIMUM_SPI_FRAME_SIZE);
}

/**
 *  @brief Create a new acknowledgement transaction (ACK or NAK) ready to transmit to the STM32.
 *
 *  This method allocates the transaction and two buffers on the heap.  It is essential
 *  that the caller cleans up both the transaction and the buffers after they are no
 *  longer necessary.
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
 *  @return pointer to spi_slave_transaction_t object
 *      This is the transaction that is returned by the ESP-IDF.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateAcknowledgementTransaction(Message *message, StatusCodes::StatusCodes code, uint8_t messageType)
{
    TRACE_MESSAGE("CreateAcknowledgementTransaction: Enter");
    spi_slave_transaction_t *transaction = CreateSpiHeaderTransaction();

    Message *copyOfMessage = message->CreateCopyOnHeap(false);
    copyOfMessage->MessageType = messageType;
    copyOfMessage->StatusCode = static_cast<uint32_t>(code);

    Encoders::EncodeMessage(copyOfMessage, _txBuffer, true);
    Message::DeleteMessage(copyOfMessage);

    TRACE_MESSAGE("CreateAcknowledgementTransaction: Exit");
    return (transaction);
}

/**
 *  @brief Setup a new transaction using the _rxBuffer and _txBuffer.
 *
 *  @returns
 *      Pointer to a transaction that can be used to send data over SPI.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateSpiHeaderTransaction()
{
    TRACE_MESSAGE("CreateSpiHeaderTransaction: Enter");

    spi_slave_transaction_t *transaction = static_cast<spi_slave_transaction_t *>(pvPortMalloc(sizeof(spi_slave_transaction_t)));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));

    ClearTransmitAndReceiveBuffers();
    transaction->rx_buffer = _rxBuffer;
    transaction->tx_buffer = _txBuffer;
    transaction->length = MAXIMUM_SPI_FRAME_SIZE * 8;
    TRACE_MESSAGE("CreateSpiHeaderTransaction: Exit");
    return (transaction);
}

/**
 *  @brief Create a new SPI transaction object that can be used to transmit the specified message body.
 *
 *  @param message
 *      The message to encode.
 *
 *  @returns
 *      Pointer to a transaction that can be used to send data over SPI.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateSpiMessageTransaction(Message *message)
{
    TRACE_MESSAGE("CreateSpiMessageTransaction: Enter");

    uint32_t encodedMessageBufferSize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE + message->PayloadLength);
    spi_slave_transaction_t *transaction = CreateSpiPayloadTransaction(encodedMessageBufferSize);
    Encoders::EncodeMessage(message, _txBuffer, false);
    TRACE_MESSAGE("CreateSpiMessageTransaction: Exit");
    return(transaction);
}

/**
 *  @brief Create a new SPI slave transmission object with zero filled buffers of the requested size.
 *
 *  @param bufferSize
 *      Number of bytes in the receive and transmit buffers.
 *
 *  @returns
 *      Pointer to a spi_slave_transaction_t object with the
 *      buffers set up and filled with zeroes.
 *
 *      The transaction object is on the heap.
 */
spi_slave_transaction_t *SpiTransportProvider::CreateSpiPayloadTransaction(uint32_t bufferSize)
{
    TRACE_MESSAGE("CreateSpiPayloadTransaction: Enter");
    TRACE_MESSAGE("CreateSpiPayloadTransaction: Creating transaction for %d bytes", bufferSize);
    spi_slave_transaction_t *transaction = static_cast<spi_slave_transaction_t *>(pvPortMalloc(sizeof(spi_slave_transaction_t)));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));
    ClearTransmitAndReceiveBuffers();

    transaction->rx_buffer = _rxBuffer;
    transaction->tx_buffer = _txBuffer;
    transaction->length = bufferSize * 8;
    TRACE_MESSAGE("CreateSpiPayloadTransaction: Exit");
    return (transaction);
}

/**
 *  @brief Release any storage associated with the SPI header transaction.
 *
 *  It is important that the storage used for the rx_buffer and tx_buffer(s)
 *  is only released if it is not the _rxBuffer and the
 *  _txBuffer as these two buffers are used frequently so
 *  they need to remain in place.
 *
 *  @param transaction
 *      Pointer to the transaction (on the heap)
 */
void SpiTransportProvider::DeleteTransaction(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("DeleteTransaction: Enter");
    if (transaction)
    {
        if (transaction->rx_buffer && (transaction->rx_buffer != _rxBuffer))
        {
            heap_caps_free(static_cast<void *>(transaction->rx_buffer));
        }
        if (transaction->tx_buffer && (transaction->tx_buffer != _txBuffer))
        {
            heap_caps_free(const_cast<void *>(transaction->tx_buffer));
        }
        vPortFree(transaction);
    }
    TRACE_MESSAGE("DeleteTransaction: Exit");
}

/**
 *  @brief SPI Post Transmission callback
 *
 *  This method is called when the STM32 has ended the SPI transmission.
 *
 *  @param transaction
 *      spi_slave_transaction_t structure containing information about the
 *      SPI transaction that has just completed.
 */
void SpiTransportProvider::SpiPostTransmissionCallback(spi_slave_transaction_t *transaction)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    gpio_set_level(Gpio::ESP32_SPI_READY_PIN, 0);
    xTaskNotifyFromISR(_transportProviderTaskHandle, TransportProviderBase::STM32_INTERRUPT_NOTIFICATION_BIT, eSetBits, &xHigherPriorityTaskWoken);
    if (_spiInstance->_synchronisationSemaphore != NULL)
    {
        xSemaphoreGiveFromISR(_spiInstance->_synchronisationSemaphore, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR();
}

/**
 *  @brief SPI Post setup callback.
 *
 *  This method is called when the SPI interface has been setup and is
 *  ready to send / receive data.
 *
 *  @param transaction
 *      spi_slave_transaction_t structure containing information about the
 *      SPI transaction that has just completed.
*/
void SpiTransportProvider::SpiPostSetupCallback(spi_slave_transaction_t *transaction)
{
    gpio_set_level(Gpio::ESP32_SPI_READY_PIN, 1);
}

/**
 *  @brief Setup the SPI transport provider.
 *
 *  @param messageDispatcher
 *      MessageDispatcher object used to send messages from the SPI
 *      interface through to the interfaces on the ESP32.
 */
void SpiTransportProvider::Setup(MessageDispatcher *messageDispatcher)
{
    TRACE_MESSAGE("Setup: Enter");
    TransportProviderBase::Setup(messageDispatcher);
    TRACE_MESSAGE("Setup: Exit");
}

/**
 *  @brief Encode a header or transport only message.
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
    TRACE_MESSAGE("EncodeHeaderMessage: Enter");
    Encoders::EncodeMessage(message, _txBuffer, true);
    memcpy(const_cast<void *>(transaction->tx_buffer), static_cast<void *>(_txBuffer), _headerFrameSize);
    TRACE_MESSAGE("EncodeHeaderMessage: Exit");
}

/**
 *  @brief Process the transport message and prepare the next transaction and
 *  change to the appropriate state.
 *
 *  @param request
 *      Inbound request from the STM32.
 *
 *  @returns
 *      Pointer to a new transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::ProcessTransportMessage(Message *request)
{
    TRACE_MESSAGE("ProcessTransportMessage: Enter");
    spi_slave_transaction_t *newTransaction = nullptr;
    switch (request->Function)
    {
        case TransportFunction::SendResponse:
            Message *message;
            message = Message::CreateOnHeap(MessageTypes::Transport | MessageTypes::Ack, Esp32Interfaces::Transport, TransportFunction::SendResponse, StatusCodes::CompletedOk);
            message->MessageID = request->MessageID;
            if (OutboundMessageQueueLength() > 0)
            {
                _responseCode = StatusCodes::CompletedOk;
                _outboundMessage = GetOutboundMessage();
                if (_outboundMessage)
                {
                    _currentState = SendingMessageDetails;
                    message->PayloadLength = _outboundMessage->PayloadLength;
                }
            }
            else
            {
                message->StatusCode = StatusCodes::NoMessagesWaiting;
            }
            newTransaction = CreateSpiHeaderTransaction();
            EncodeHeaderMessage(newTransaction, message);
            Message::DeleteMessage(message);
            _currentState = SendingMessageDetails;
            break;
        default:
            TRACE_MESSAGE("ProcessTransportMessage: Transport function %d not recognised.", request->Function);
            _responseCode = StatusCodes::CompletedOk;
            _messageType = MessageTypes::Nak;
            break;
    }
    TRACE_MESSAGE("ProcessTransportMessage: Exit");
    return(newTransaction);
}

/******************************************************************************
 *
 * The following four methods process the incoming messages for processing  by
 * the ESP32.
 *
 * The basic process is as follows:
 *  - Header message is received
 *  - Header is acknowledged (ACK or NAK)
 *  - Message body is sent (optional)
 *  - Message body is acknowledged (optional)
 *
 * The header message gives information about the message being transmitted
 * with the most critical piece of information being the length of the
 * message payload.
 *
 * A payload length of 0 indicates the that the message can be processed
 * without a payload.  Actions such as GetAntenna do not need a payload to
 * determine the action to be taken.
 *
 * If the payload length is non-zero then the message body will contain the
 * parameters needed to perform the appropriate action.  In this case the
 * message body is requested.
 *
 * Both message headers and message bodies contain a 32-bit CRC to verify that
 * the data has been transmitted and received correctly.  A message ACK is
 * sent to the STM32 if the CRC in the message matches the calculated CRC.  A
 * NAK message is sent to the STM32 if the CRCs do not match.
 *
 ******************************************************************************/

/**
 *  @brief The STM32 has sent what is assumed to be a message header to the ESP32.
 *  Work out what is the appropriate next action.
 *
 *  @param transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      New SPI transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::MessageReceived(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("InboundMessageReceived: Entering method");
    if (_incomingMessage)
    {
        Message::DeleteMessage(_incomingMessage);
        _incomingMessage = nullptr;
    }

    spi_slave_transaction_t *newTransaction = nullptr;
    _currentState = AcknowledgingMessage;
    int dataSize = transaction->trans_len / 8;
    TRACE_MESSAGE("InboundMessageReceived: Received %d bytes.", dataSize);
    TRACE_HEX_BUFFER(transaction->rx_buffer, dataSize);
    if (dataSize >= _headerFrameSize)
    {
        _incomingMessage = Encoders::ExtractMessage(static_cast<uint8_t *>(transaction->rx_buffer), dataSize, false);
        if (!_incomingMessage)             // nullptr indicates a CRC error.
        {
            ERROR_MESSAGE("InboundMessageReceived: CRC Error.");
            _responseCode = StatusCodes::CrcError;
            _messageType = MessageTypes::Nak;
            _incomingMessage = Message::CreateOnHeap(_messageType, Esp32Interfaces::Transport, 0, _responseCode);
            newTransaction = CreateAcknowledgementTransaction(_incomingMessage, _responseCode, _messageType);
        }
        else
        {
            switch (_incomingMessage->MessageType)
            {
                case MessageTypes::Header:
                    if (_incomingMessage->Interface != Esp32Interfaces::Transport)
                    {
                        _messageDispatcher->QueueMessageForEsp32(_incomingMessage);
                    }
                    _messageType = MessageTypes::Ack;
                    _responseCode = StatusCodes::CompletedOk;
                    newTransaction = CreateAcknowledgementTransaction(_incomingMessage, _responseCode, _messageType);
                    break;
                case MessageTypes::Transport:
                    newTransaction = ProcessTransportMessage(_incomingMessage);
                    Message::DeleteMessage(_incomingMessage);
                    _incomingMessage = nullptr;
                    break;
                default:
                    _responseCode = StatusCodes::UnexpectedData;
                    _messageType = MessageTypes::Nak;
                    Message::DeleteMessage(_incomingMessage);
                    _incomingMessage = Message::CreateOnHeap(_messageType, Esp32Interfaces::Transport, 0, _responseCode);
                    newTransaction = CreateAcknowledgementTransaction(_incomingMessage, _responseCode, _messageType);
                    break;
            }
        }
    }
    else
    {
        _responseCode = StatusCodes::InvalidHeader;
        _messageType = MessageTypes::Nak;
        _incomingMessage = Message::CreateOnHeap(_messageType, Esp32Interfaces::Transport, 0, _responseCode);
        newTransaction = CreateAcknowledgementTransaction(_incomingMessage, _responseCode, _messageType);
    }
    DeleteTransaction(transaction);
    TRACE_MESSAGE("InboundMessageReceived: Exit");
    return(newTransaction);
}

/**
 *  @brief The inbound message has been acknowledged so work out if the message is
 *  complete (i.e. no payload) and so can be queued or if we need to request
 *  the full message.
 *
 *  @param transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      New SPI transaction.
 */
spi_slave_transaction_t *SpiTransportProvider::MessageAcknowledged(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("MessageAcknowledged: Entering method");

    DeleteTransaction(transaction);
    spi_slave_transaction_t *newTransaction = CreateSpiHeaderTransaction();
    _incomingMessage = nullptr;
    _currentState = WaitingForMessage;
    
    TRACE_MESSAGE("MessageAcknowledged: Exit");
    return(newTransaction);
}

/******************************************************************************
 *
 * The following four methods process the outbound messages for processing  by
 * the STM32.
 *
 * The basic process is as follows:
 *  - Header message requested by the STM32
 *  - Header is sent to the STM32
 *  - Header message is acknowledged by the STM32
 *  - Message body is sent to the STM32 (optional)
 *  - Message body is acknowledged by the STM32 (optional)
 *
 * The process for message exchange is very similar to the STM32 sending
 * messages to the ESP32 (see comment above for the inbound messages for
 * more information).  The main difference is the fact that the process is
 * triggered by an incoming Transport message requesting a response.
 *
 ******************************************************************************/

/**
 *  @brief The message header will already have been sent, we need to get ready to
 *  receive and ACK or NAK from the STM32.
 *
 *  @param transaction
 *      Current SPI transaction used to get data from the STM32.
 *
 *  @returns
 *      SPI transaction created to receive the message acknowledgement.
 */
spi_slave_transaction_t *SpiTransportProvider::SendMessage(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("MessageDetailsSent: Enter");
    DeleteTransaction(transaction);
    //
    //  TODO: Deal with errors (outbound message is null etc.)
    //
    spi_slave_transaction_t *newTransaction = nullptr;
    if (_outboundMessage)
    {
        TRACE_MESSAGE("Sending outbound message");
        newTransaction = CreateSpiMessageTransaction(_outboundMessage);
        _currentState = SendingMessage;
    }
    else
    {
        TRACE_MESSAGE("Outbound message is null");
    }
    TRACE_MESSAGE("MessageDetailsSent: Exit");
    return(newTransaction);
}

/**
 *  @brief Implement processing of the incoming message header acknowledgement.
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
 *  @returns
 *      New SPI transaction that can be used to communicate with the STM32.
 *
 */
spi_slave_transaction_t *SpiTransportProvider::MessageSent(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("MessageSent: Enter");
    // Message *acknowledgement = Encoders::ExtractMessage(static_cast<uint8_t *>(transaction->rx_buffer), _headerFrameSize, true);

    DeleteTransaction(transaction);
    spi_slave_transaction_t *newTransaction = nullptr;

    // if (acknowledgement)
    // {
        // TRACE_MESSAGE("MessageSent: Acknowledgement OK");
        // if (acknowledgement->MessageType == MessageTypes::Ack)
        // {
            TRACE_MESSAGE("MessageSent: Preparing message transaction.");
            // if (_outboundMessage->PayloadLength > 0)
            // {
                newTransaction = CreateSpiMessageTransaction(_outboundMessage);
                _currentState = WaitingForMessageAcknowledgement;
            // }
            // else
            // {
            //     TRACE_MESSAGE("Payload length > 0");
            //     Message::DeleteMessage(_outboundMessage);
            //     newTransaction = CreateSpiHeaderTransaction();
            //     _currentState = WaitingForMessage;
            //     _outboundMessage = nullptr;
            // }
        // }
        // else
        // {
        //     TRACE_MESSAGE("Not an ACK.");
        //     QueueMessageForStm32(_outboundMessage);
        //     _outboundMessage = nullptr;
        //     newTransaction = CreateSpiHeaderTransaction();
        //     _currentState = WaitingForMessage;
        // }
        // Message::DeleteMessage(acknowledgement);
    // }
    // else
    // {
    //     TRACE_MESSAGE("MessageSent: Acknowledgement is null");
    //     QueueMessageForStm32(_outboundMessage);
    //     newTransaction = CreateSpiHeaderTransaction();
    //     _currentState = WaitingForMessage;
    //     _outboundMessage = nullptr;
    // }
    TRACE_MESSAGE("MessageSent: Exit");
    return(newTransaction);
}

/**
 *  @brief Implement processing of the out-bound message body acknowledgement.
 *
 *  The message body should have been sent and the STM32 will have responded
 *  with an ACK or NAK.  If the message has failed (i.e. corrupt) then the
 *  ESP should just put the message back on the queue.
 *
 *  If the body was received then free up any memory used and prepare to
 *  receive the next message header.
 *
 *  Note: Contains HEAPCODE for heap tracing / debugging.
 *
 *  @param transaction
 *      Last SPI transaction that was executed.
 *
 *  @returns
 *      New SPI transaction with data.
 */
spi_slave_transaction_t *SpiTransportProvider::AcknowledgementReceived(spi_slave_transaction_t *transaction)
{
    TRACE_MESSAGE("AcknowledgementReceived: Enter");
    Message *incomingMessage = Encoders::ExtractMessage(static_cast<uint8_t *>(transaction->rx_buffer), _headerFrameSize, true);
    DeleteTransaction(transaction);
    if (incomingMessage)
    {
        if (incomingMessage->MessageType == MessageTypes::Ack)
        {
            #if defined(PERFORMANCE_LOGGING)
            PERFORMANCE_LOGGING_EXIT();
            Logging::AddPerformanceCounterEntry(Logging::MessageSummary(_outboundMessage), _outboundMessage->PerformanceLoggingStart, performance_logging_end);
            #endif

            Message::DeleteMessage(_outboundMessage);
            TRACE_MESSAGE("Message request processed.");
        }
        else
        {
            TRACE_MESSAGE("Message is not ACK %02x", incomingMessage->MessageType);
            QueueMessageForStm32(_outboundMessage);
        }
    }
    else
    {
        TRACE_MESSAGE("Incoming message cannot be decoded.");
        QueueMessageForStm32(_outboundMessage);
    }
    Message::DeleteMessage(incomingMessage);
    _currentState = WaitingForMessage;
    _outboundMessage = nullptr;

    TRACE_MESSAGE("AcknowledgementReceived: Exit");
    return(CreateSpiHeaderTransaction());
}

/**
 *  @brief Check to see if the message buffer contains any data.
 *
 *
 *  This method simply scans a buffer that should contain an encoded message
 *  to see if it contains any non-zero values.  This method is necessary
 *  because of a fault that occasionally occurs where the interrupt that
 *  indicates that the ESP32 has received data fires when it should not as
 *  there is no message.
 *
 *  A message cannot be all zeros as there must be non-zero bytes in the CRC.
 *
 *  @param buffer
 *      Pointer to a buffer of data.
 *
 *  @param bufferLength
 *      Number of bytes in the buffer.
 *
 *  @returns
 *      true if the message contains valid (i.e. non-zero bytes) data.
 *      false if the buffer is not a valid encoded message.
 */
bool SpiTransportProvider::MessageContainsData(uint8_t *buffer, uint32_t bufferLength)
{
    TRACE_MESSAGE("MessageContainsData: Enter");
    bool result = false;
    for (uint32_t index = 0; index < bufferLength; index++)
    {
        if (buffer[index] != 0)
        {
            result = true;
            break;
        }
    }
    TRACE_MESSAGE("MessageContainsData: Exit");
    return(result);
}

/**
 *  @brief Check to see if the ESP32 is sending data.
 *
 *  @param state
 *      State to check.
 *
 *  @returns
 *      true if the state indicates that the ESP32 is sending data,
 *      false otherwise.
 */
bool SpiTransportProvider::SendingData(SpiStates state)
{
    TRACE_MESSAGE("SendingData: Enter");
    TRACE_MESSAGE("SendingData: Exit");
    return((_currentState == AcknowledgingMessage)
            || (_currentState == SendingMessageDetails)
            || (_currentState == SendingMessage));
}

/**
 *  @brief Start the SPI communication and with for a signal that we have messages to process.
 *  Process the incoming message and return a new transaction that can be used for the
 *  next stage in the process.
 *
 *  @param transaction
 *      Valid SPI transaction that can be used to communicate with the STM32.
 *
 *  @returns
 *      Newly setup SPI transaction that can be used in the next stage of the process.
 *
 */
spi_slave_transaction_t *SpiTransportProvider::ProcessMessages(spi_slave_transaction_t *transaction)
{
    spi_slave_transaction_t *newTransaction = nullptr;
    uint32_t notificationBits = 0;

    TRACE_MESSAGE("ProcessMessages: Waiting for notification, current state: %s", SpiStateString(_currentState));
    ESP_ERROR_CHECK(spi_slave_queue_trans(VSPI_HOST, transaction, portMAX_DELAY));
    xTaskNotifyWait(0x00000000, ULONG_MAX, &notificationBits, portMAX_DELAY);

    //
    //  ESP-IDF documentation states that a call to spi_slave_get_trans_result
    //  is mandatory when spi_slave_queue_trans is used.
    //
    ESP_ERROR_CHECK(spi_slave_get_trans_result(VSPI_HOST, &transaction, portMAX_DELAY));
    if (SendingData(_currentState) || MessageContainsData(static_cast<uint8_t *>(transaction->rx_buffer), transaction->length / 8))
    {
        switch (_currentState)
        {
            case WaitingForMessage:
                newTransaction = MessageReceived(transaction);
                break;
            case AcknowledgingMessage:
                newTransaction = MessageAcknowledged(transaction);
                break;

            case SendingMessageDetails:
                newTransaction = SendMessage(transaction);
                break;
            case SendingMessage:
                newTransaction = MessageSent(transaction);
                break;
            case WaitingForMessageAcknowledgement:
                newTransaction = AcknowledgementReceived(transaction);
                break;

            default:
                ERROR_MESSAGE("ProcessMessages: Unhandled state: %s", SpiStateString(_currentState));
                break;
        }
    }
    else
    {
        TRACE_MESSAGE("ProcessMessages: Empty buffer, discarding data and staying in state %s", SpiStateString(_currentState));
        newTransaction = transaction;
    }
    return(newTransaction);
}

/**
 *  @brief Process any messages for the controller.
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
    SpiTransportProvider *sender = static_cast<SpiTransportProvider *>(pvParameters);

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
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(sender->_transportProviderTaskHandle);
}

/**
 *  @brief Task to send synchronisation pulses to the STM32.
 *
 *  There can sometimes be a problem with the synchronisation between the ESP32 and the
 *  STM32.  This arises due to the real-time nature of the startup process.  So the signal
 *  the STM32 indicating the readiness of the ESP32 may be missed as the interrupt
 *  handler on the STM32 is not set up when the ESP32 sends the "Message waiting"
 *  signal to the STM32.
 *
 *  To overcome this, this task will send the message waiting signal to the STM32 every 500ms.
 *  The binary synchronisation semaphore indicates if the request has been made or not.
 *
 *  Note that this method is static in order to allow the C methods in
 *  FreeRTOS to call the method.
 *
 *  @param pvParameters
 *      Pointer to the instance of the request handler.
 */
void SpiTransportProvider::SynchronisationTask(void *pvParameters)
{
    SpiTransportProvider *sender = static_cast<SpiTransportProvider *>(pvParameters);
    TRACE_MESSAGE_SPECIFY_COMPONENT(sender->COMPONENT_NAME, "Synchronising with the STM32.");
    //
    //  It is important to wait for the outbound message queue to be
    //  created before we try to process any messages.
    //
    while (xSemaphoreTake(sender->_synchronisationSemaphore, pdMS_TO_TICKS(2000)) != pdTRUE)
    {
        TRACE_MESSAGE_SPECIFY_COMPONENT(sender->COMPONENT_NAME, "Sending synchronisation pulse to STM32.");
        sender->ToggleEsp32MessageWaitingPin();
    }
    vSemaphoreDelete(sender->_synchronisationSemaphore);
    sender->_synchronisationSemaphore = NULL;

    TRACE_MESSAGE_SPECIFY_COMPONENT(sender->COMPONENT_NAME, "Synchronisation complete.");

    vTaskDelete(sender->_synchronisationTaskHandle);
    sender->_synchronisationTaskHandle = NULL;
}
