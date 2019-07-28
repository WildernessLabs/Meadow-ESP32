/*
 *      SpiTransportProvider.cpp
 *
 *      Hardware transport provider for messages between the ESP32 and
 *      the STM32 using the SPI interface.
 */
#include "SpiTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include <string.h>

/*
 * ----------------------------------------------------------------------------
 *
 *                   Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Default state is that the class has not been fully constructed
 *      and so there is no current instance.
 */
SpiTransportProvider *SpiTransportProvider::_spiInstance = nullptr;

/*
 *      Initial state of the system is Invalid.  The constructor will set
 *      up the SPI interface and then move the state to one where we can
 *      receive data.
 */
SpiTransportProvider::SpiStates SpiTransportProvider::_currentState = Invalid;

/*
 *      Buffers will be allocated as needed and so they are initially null.
 */
uint8_t *SpiTransportProvider::_headerReceiveBuffer = nullptr;
uint8_t *SpiTransportProvider::_headerTransmitBuffer = nullptr;
uint8_t *SpiTransportProvider::_transportFrameBuffer = nullptr;
uint32_t SpiTransportProvider::_headerFrameSize = 0;

/*
 * ---------------------------------------------------------------------------
 *
 *                         Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/*
 *      Default constructor for the SPI transport provider.
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
     *  Configure the STM32 Ready signal pin for interrupts.
     *
     *  Interrupt on rising edges on the STM32_READY_PIN.  The pin has
     *  pull-up and pull-downs disabled.
     *
     */
//    gpio_config_t stm32ReadyPinConfig;
//    stm32ReadyPinConfig.intr_type = GPIO_INTR_POSEDGE;
//    stm32ReadyPinConfig.pin_bit_mask = (1ULL << Gpio::STM32_READY_PIN);
//    stm32ReadyPinConfig.mode = GPIO_MODE_INPUT;
//    stm32ReadyPinConfig.pull_up_en = GPIO_PULLUP_DISABLE;
//    stm32ReadyPinConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
//    ESP_ERROR_CHECK(gpio_config(&stm32ReadyPinConfig));
//    ESP_ERROR_CHECK(gpio_install_isr_service(0));
//    ESP_ERROR_CHECK(gpio_isr_handler_add(Gpio::STM32_READY_PIN, Stm32IsrHandler, this));
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
    memset(_headerReceiveBuffer, 0, _headerFrameSize);
    memset(_headerTransmitBuffer, 0, _headerFrameSize);

    _currentState = Idle;

    /*
     *  Set up the task and that will process the SPI messages.
     */
    xTaskCreate(Task, "SPIMessageTask", 2048 * 2, this, configMAX_PRIORITIES - 1, &SpiTransportProvider::_transportProviderTaskHandle);
}

/*
 *      Default destructor for the SPI transport provider.
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
 *                                  Methods
 *
 * ---------------------------------------------------------------------------
 */

/*
 *      Convert a state into a meaningfull message for debugging.
 *
 *      @param state
 *          State to be decoded.
 *
 *      @returns
 *          Text representation of an SpiState.
 */
const char *SpiTransportProvider::SpiStateString(SpiTransportProvider::SpiStates state)
{
    const char *stateString = NULL;

    switch (state)
    {
        case Invalid:
            stateString = "Invalid";
            break;
        case Idle:
            stateString = "Idle";
            break;
        case AcknowledgingMessageHeader:
            stateString = "AcknowledgingMessageHeader";
            break;
        case WaitingForMessageBody:
            stateString = "WaitingForMessageBody";
            break;
        case AcknowledingMessageBody:
            stateString = "AcknowledingMessageBody";
            break;
        case WantToSendMessageHeader:
            stateString = "WantToSendMessageHeader";
            break;
        case WaitingForMessageHeaderAcknowledgement:
            stateString = "WaitingForMessageHeaderAcknowledgement";
            break;
        case WantToSendMessageBody:
            stateString = "WantToSendMessageBody";
            break;
        case WaitingForMessageBodyAcknowledgement:
            stateString = "WaitingForMessageBodyAcknowledgement";
            break;
        case WantToSendTransportMessage:
            stateString = "WantToSendTransportMessage";
            break;
    }
    return (stateString);
}

/*
 *      Clear the transaction receive and transmit buffers.
 *
 *      Note that it is assumed that the receive and transmit buffers are the
 *      same size.
 *
 *      @param receiveBuffer
 *          Pointer to the buffer that will hold the data that will be received.
 *
 *      @param transmitBuffer
 *          Pointer to the buffer that will hold the data to be sent.
 *
 *      @param bufferSize
 *          Number of bytes in the receive and transmit buffers.
 */
void SpiTransportProvider::ClearBuffers(uint8_t *receiveBuffer, uint8_t *transmitBuffer, uint32_t bufferSize)
{
    memset(receiveBuffer, 0, bufferSize);
    memset(transmitBuffer, 0, bufferSize);
}

/*
 *      Start the acknowledgement process.
 *
 *      @param message
 *          Message to acknowledge.
 *
 *      @param code
 *          Status code to return.
 *
 *      @param messageType
 *          Type of message to return.
 *
 *      @returns spi_slave_transaction_t object
 *          This is the transaction that is returned by the ESP-IDF.
 *
 *      @Notes
 *          This method allocates the transaction and two buffers on the heap.  It is essential
 *          that the caller cleans up both the transaction and the buffers after they are no
 *          longer necessary.
 */
spi_slave_transaction_t *SpiTransportProvider::AcknowledgeMessage(Message *message, StatusCodes::StatusCodes code, uint8_t messageType)
{
    spi_slave_transaction_t *transaction = SetupSpiHeaderTransaction();

    Message *copyOfMessage = Message::CopyMessage(message, false);
    copyOfMessage->MessageType = messageType;
    copyOfMessage->StatusCode = (uint32_t) code;
    uint8_t *ackFrame = (uint8_t *) Encoders::EncodeMessage(copyOfMessage, true);
    DeleteMessageAndReleaseHeapStorage(copyOfMessage);
    memcpy((void *) transaction->tx_buffer, ackFrame, _headerFrameSize);
    vPortFree(ackFrame);
//
//    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
//    gpio_set_level(Gpio::ESP32_READY_PIN, 0);
//
    return (transaction);
}

/*
 *      Setup a new transaction using the _headerReceiveBuffer and
 *      _headerTransmitBuffer.
 */
spi_slave_transaction_t *SpiTransportProvider::SetupSpiHeaderTransaction()
{
    spi_slave_transaction_t *transaction = (spi_slave_transaction_t *) pvPortMalloc(sizeof(spi_slave_transaction_t));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));
    ClearBuffers(_headerReceiveBuffer, _headerTransmitBuffer, _headerFrameSize);
    transaction->rx_buffer = _headerReceiveBuffer;
    transaction->tx_buffer = _headerTransmitBuffer;
    transaction->length = _headerFrameSize * 8;

    _spiTransaction = transaction;
    return (transaction);
}

/*
 *      Create a new SPI slave transmission object with zero filled
 *      buffers of the requested size.
 *
 *      @param bufferSize
 *          Number of bytes in the receive and transmit buffers.
 *
 *      @returns
 *          Pointer to a spi_slave_transaction_t object with the
 *          buffers set up and filled with zeroes.
 */
spi_slave_transaction_t * SpiTransportProvider::SetupSpiPayloadTransaction(uint32_t bufferSize)
{
    spi_slave_transaction_t *transaction = (spi_slave_transaction_t *) pvPortMalloc(sizeof(spi_slave_transaction_t));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));
    uint8_t *transmitBuffer = (uint8_t *) heap_caps_calloc(bufferSize, 1, MALLOC_CAP_DMA);
    uint8_t *receiveBuffer = (uint8_t *) heap_caps_calloc(bufferSize, 1, MALLOC_CAP_DMA);
    ClearBuffers(receiveBuffer, transmitBuffer, bufferSize);

    transaction->rx_buffer = receiveBuffer;
    transaction->tx_buffer = transmitBuffer;
    transaction->length = bufferSize * 8;

    _spiTransaction = transaction;
    return (transaction);
}

/*
 *      Release any storage allocated on the heap associated
 *      with an SPI transaction.
 *
 *      @param transaction
 *          Pointer to the transaction (on the heap)
 *
 *      @param currentState
 *          Current state of the state machine.
 */
void SpiTransportProvider::FreeUpTransactionHeapStorage(spi_slave_transaction_t *transaction, SpiStates currentState)
{
    //
    //  We need to delete the Rx and Tx buffers of the SPI transaction
    //  when we are NOT using the header buffers.
    //
    if (currentState != Idle)
    {
        heap_caps_free((void *) transaction->rx_buffer);
        heap_caps_free((void *) transaction->tx_buffer);
    }
    vPortFree(transaction);
}

/*
 *      SPI Post Transmission callback
 *
 *      This method is called when the SPI interface has received
 *      the full data load from the STM32.
 *
 *      @param transaction
 *          spi_slave_transaction_t structure containing information about the
 *          SPI transaction that has just completed.
 */
void SpiTransportProvider::SpiPostTransmissionCallback(spi_slave_transaction_t *transaction)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(_transportProviderTaskHandle, TransportProviderBase::STM32_INTERRUPT_NOTIFICATION_BIT, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR();
}

/*
 *      SPI Post setup callback
 *
 *      This method is called when the SPI interface has completed
 *      setup and data is ready for transmission.
 *
 *      @param transaction
 *          spi_slave_transaction_t structure containing information about the
 *          SPI transaction that has just completed.
 */
void SpiTransportProvider::SpiPostSetupCallback(spi_slave_transaction_t *transaction)
{
}

/*
 *      Setup the SPI transport provider.
 *
 *      @param messageDispatcher
 *          MessageDispatcher object used to send messages from the SPI
 *          interface through to the interfaces on the ESP32.
 */
void SpiTransportProvider::Setup(MessageDispatcher *messageDispatcher)
{
    TransportProviderBase::Setup(messageDispatcher);
}

/*
 *      The Echo function simply sends a message through the various communication layers.
 *      This is a test function to verify correct communication through the message
 *      stack.
 *
 *      @param messageString
 *          Message to send through the communication layers.
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
 *      Clear the class level variable _currentHeader.
 *
 *      This method ensures that any heap storage is released and the
 *      pointer is set to the nullptr value.
 *
 *      @param message
 *          Pointer to the message to be cleared.
 */
void SpiTransportProvider::DeleteMessageAndReleaseHeapStorage(Message *message)
{
    if (message)
    {
        if (message->Payload)
        {
            vPortFree(message->Payload);
        }
        vPortFree(message);
    }
}

/*
 *      The STM32 has indicated that it has a message for the ESP32.
 *
 *      This method requests header information from the STM32 and starts
 *      the request process.
 */
void SpiTransportProvider::ProcessIdleState(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "Entering Method.");

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);

    Message *messageHeader;
    uint8_t messageType = MessageTypes::Ack;
    StatusCodes::StatusCodes responseCode = StatusCodes::CompletedOK;
    if (transaction->trans_len >= (Message::HEADER_SIZE * 8))
    {
        messageHeader = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, _headerFrameSize, true);
        if (!messageHeader)             // nullptr indicates a CRC error.
        {
            responseCode = StatusCodes::CRCError;
            messageType = MessageTypes::Nak;
            messageHeader = Message::CreateOnHeap(messageType, Esp32Interfaces::Transport, 0, responseCode, 0, nullptr, 0);
        }
    }
    else
    {
        responseCode = StatusCodes::InvalidHeader;
        _messageType = MessageTypes::Nak;
        messageHeader = Message::CreateOnHeap(messageType, Esp32Interfaces::Transport, 0, responseCode, 0, nullptr, 0);
    }
    FreeUpTransactionHeapStorage(transaction, Idle);
    DeleteMessageAndReleaseHeapStorage(_incomingMessage);
    _incomingMessage = messageHeader;
    _responseCode = responseCode;

    ESP_LOGI(__func__, "Preparing response, packet type: %d, status code: %d", messageType, responseCode);
    _spiTransaction = AcknowledgeMessage(_incomingMessage, responseCode, messageType);
    _currentState = AcknowledgingMessageHeader;
}

/*
 *      Acknowledge the message header that has been received and work out the next steps.
 *
 *      If the header is valid then the response will be an acknowledgement and
 *          - Messages with no payload should be queued
 *          - Messages with payloads should enter the waiting for message body state.
 *
 *      If the header was corrupt then there is little we can do other than send a NAK
 *      message and then throw away the data and wait for the sender to restart the
 *      sequence.
 *
 *      Upon entry it is essential that the following variables have been set up:
 *          _currentHeader
 *          _responseCode
 *          _messageType
 */
void SpiTransportProvider::ProcessAcknowledgingMessageHeaderState()
{
    ESP_LOGI(__func__, "Entering Method.");

//    spi_slave_transaction_t *transaction = AcknowledgeMessage(_incomingMessage, _responseCode, _messageType);

    if (_responseCode == StatusCodes::CompletedOK)
    {
        if (_incomingMessage->PayloadLength == 0)         // Assume _currentHeader has been set up.
        {
            //
            //  If we get here then the message is simple, i.e. no payload
            //  so we need to queue the message on the appropriate interface.
            //
            if (_incomingMessage->Interface != Esp32Interfaces::Transport)
            {
                _messageDispatcher->QueueMessageForEsp32(_incomingMessage);
            }
            SetupSpiHeaderTransaction();
            _currentState = Idle;
        }
        else
        {
            _currentState = WaitingForMessageBody;
        }
    }
    else
    {
        DeleteMessageAndReleaseHeapStorage(_incomingMessage);
        _incomingMessage = nullptr;
        SetupSpiHeaderTransaction();
        _currentState = Idle;
    }
//    FreeUpTransactionHeapStorage(transaction, _currentState);
}

/*
 *      Previous messages have indicated that a message body is required, request the message body.
 *
 *      This method will set up the response types but will leave the _currentHeader variable in place.
 *      Memory allocated to _currentHeader must be deallocated in the Acknowledgement method.
 */
void SpiTransportProvider::ProcessWaitingForMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");

    uint32_t encodedMessageBodySize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE + _incomingMessage->PayloadLength);
    spi_slave_transaction_t *transaction = SetupSpiPayloadTransaction(encodedMessageBodySize);

    ESP_LOGI(__func__, "Current header payload length: %d", _incomingMessage->PayloadLength);
    ESP_LOGI(__func__, "Message body buffer size: %d", encodedMessageBodySize);

    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
    Message *message = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, encodedMessageBodySize, false);

    if (message)
    {
        if (dataSize != encodedMessageBodySize)
        {
            _responseCode = StatusCodes::InvalidPacket;
            _messageType = MessageTypes::Nak;
        }
        else
        {
            if (message->MessageType == MessageTypes::Data)
            {
                if ((message->Interface == _incomingMessage->Interface) && (message->Function == _incomingMessage->Function) && (message->MessageID == _incomingMessage->MessageID))
                {
                    _responseCode = StatusCodes::CompletedOK;
                    _messageType = MessageTypes::Ack;
                    _messageDispatcher->QueueMessageForEsp32(message);
                }
                else
                {
                    _responseCode = StatusCodes::UnexpectedData;
                    _messageType = MessageTypes::Nak;
                }
            }
            else
            {
                _responseCode = StatusCodes::UnexpectedData;
                _messageType = MessageTypes::Nak;
            }
        }
    }
    else
    {
        _responseCode = StatusCodes::CRCError;
        _messageType = MessageTypes::Nak;
    }
    FreeUpTransactionHeapStorage(transaction, _currentState);
    _currentState = AcknowledingMessageBody;
}

/*
 *      Acknowledge the receipt of the message body.
 *
 *      This method requires the following class level variables to have
 *      been setup correctly:
 *
 *          _currentHeader
 *          _responseCode
 *          _messageType
 */
void SpiTransportProvider::ProcessAcknowledingMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t *transaction = AcknowledgeMessage(_incomingMessage, _responseCode, _messageType);
    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);

    FreeUpTransactionHeapStorage(transaction, _currentState);
    DeleteMessageAndReleaseHeapStorage(_incomingMessage);
    _incomingMessage = nullptr;
    _currentState = Idle;
}

/*
 *      Send a message header to the STM32.
 */
void SpiTransportProvider::ProcessWantToSendMessageHeaderState()
{
    ESP_LOGI(__func__, "Entering Method.");

    if (_outboundMessage)
    {
        uint8_t *encodedMessageHeader = Encoders::EncodeMessage(_outboundMessage, true);

        ESP_LOGI(__func__, "Want to send the following message:");
        ESP_LOG_BUFFER_HEX(__func__, encodedMessageHeader, Message::HEADER_SIZE);

        spi_slave_transaction_t *transaction = SetupSpiPayloadTransaction(_headerFrameSize);
        memcpy((void *) transaction->tx_buffer, (void *) encodedMessageHeader, Message::HEADER_SIZE);

        gpio_set_level(Gpio::ESP32_READY_PIN, 1);
        ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
        gpio_set_level(Gpio::ESP32_READY_PIN, 0);

        int dataSize = transaction->length / 8;
        ESP_LOGI(__func__, "Received %d bytes.", dataSize);
        ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
        Message *incomingMessage = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, _headerFrameSize, true);
        if (incomingMessage)
        {
            if (incomingMessage->MessageType & MessageTypes::Ready)
            {
                _currentState = WaitingForMessageHeaderAcknowledgement;
            }
            else
            {
                //  TODO: we are expecting a Ready message and we have received something else.
            }
        }
        else
        {
            //  TODO: We must have received a corrupt message (CRC error).
        }
        FreeUpTransactionHeapStorage(transaction, _currentState);
    }
    else
    {
        ESP_LOGE(__func__, "No message to send.");
        _currentState = Idle;
    }
}

/*
 *      Implement processing of the incoming message header acknowledgement messages.
 */
void SpiTransportProvider::ProcessWaitingForMessageHeaderAcknowledgementState()
{
    ESP_LOGI(__func__, "Entering Method.");
    spi_slave_transaction_t *transaction = SetupSpiPayloadTransaction(_headerFrameSize);

    gpio_set_level(Gpio::ESP32_READY_PIN, 1);
    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
    Message *incomingMessage = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, _headerFrameSize, true);
    if (incomingMessage)
    {
        if (incomingMessage->MessageType == MessageTypes::Ack)
        {
            ESP_LOGI(__func__, "Acknowledgement received.");
            //
            //  TODO: check if there is a message body ?
            //
            _currentState = WantToSendMessageBody;
        }
    }
    else
    {
        //  TODO: Deal with corrupt messages.
        _currentState = Idle;
    }
    FreeUpTransactionHeapStorage(transaction, _currentState);
//    DeleteMessageAndReleaseHeapStorage(_incomingMessage);
//    _incomingMessage = nullptr;
}

/*
 *      Send a message body to the STM32.
 */
void SpiTransportProvider::ProcessWantToSendMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");
    if (_outboundMessage)
    {
        uint8_t *encodedMessageBody = Encoders::EncodeMessage(_outboundMessage, false);
        ESP_LOGI(__func__, "Want to send:");
        ESP_LOG_BUFFER_HEX(__func__, encodedMessageBody, Message::HEADER_SIZE + _outboundMessage->PayloadLength);

        uint32_t encodedMessageBufferSize = Encoders::CalculateSpiBufferSize(Message::HEADER_SIZE + _outboundMessage->PayloadLength);
        spi_slave_transaction_t *transaction = SetupSpiPayloadTransaction(encodedMessageBufferSize);
        memcpy((void *) transaction->tx_buffer, (void *) encodedMessageBody, Message::HEADER_SIZE + _outboundMessage->PayloadLength);

        gpio_set_level(Gpio::ESP32_READY_PIN, 1);
        ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
        gpio_set_level(Gpio::ESP32_READY_PIN, 0);

        int dataSize = transaction->length / 8;
        ESP_LOGI(__func__, "Received %d bytes.", dataSize);
        ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
        _currentState = WaitingForMessageBodyAcknowledgement;
        FreeUpTransactionHeapStorage(transaction, _currentState);
    }
    else
    {
        ESP_LOGE(__func__, "No message to send.");
        _currentState = Idle;
    }
}

/*
 *      Implement processing of the out-bound message body acknowledgement.
 */
void SpiTransportProvider::ProcessWaitingForMessageBodyAcknowledgementState()
{
    ESP_LOGI(__func__, "Entering Method.");
    spi_slave_transaction_t *transaction = SetupSpiPayloadTransaction(_headerFrameSize);

    gpio_set_level(Gpio::ESP32_READY_PIN, 1);
    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
    Message *incomingMessage = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, _headerFrameSize, true);
    if (incomingMessage)
    {
        if (incomingMessage->MessageType == MessageTypes::Ack)
        {
            ESP_LOGI(__func__, "Acknowledgement received.");
            DeleteMessageAndReleaseHeapStorage(_outboundMessage);
            _outboundMessage = nullptr;
            _currentState = Idle;
        }
    }
    else
    {
        //  TODO: Deal with corrupt messages.
        _currentState = Idle;
    }
    FreeUpTransactionHeapStorage(transaction, _currentState);
}

/*
 *      Start the transport message transmission process.
 */
void SpiTransportProvider::ProcessWantToSendTransportMessageState()
{
    ESP_LOGI(__func__, "Entering Method.");
}

/*
 *  Wait for an incoming notification and work out if we should be entering a send
 *  or receive state.
 */
void SpiTransportProvider::WaitForNotification()
{
    uint32_t notificationBits = 0;

    ESP_LOGI(__func__, "Waiting for notification, current state: %s", SpiStateString(_currentState));
    xTaskNotifyWait(0x00000000, 0x00000000, &notificationBits, portMAX_DELAY);

    if ((_currentState == SpiStates::Idle) && (notificationBits & TransportProviderBase::OUTBOUND_MESSAGE_QUEUE_NOTIFICATION_BIT))
    {
        if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
        {
            UBaseType_t messageCount = uxQueueMessagesWaiting(_outboundMessageQueueHandle);
            ESP_LOGI(__func__, "Messages waiting: %d", messageCount);
            if (messageCount > 0)
            {
                if (xQueueReceive(_outboundMessageQueueHandle, &_outboundMessage, portMAX_DELAY) == pdTRUE)
                {
                    _currentState = SpiStates::WantToSendMessageHeader;
                }
            }
            xSemaphoreGive(_outboundMessageQueueMutex);
        }
    }
}

/*
 *      Processes messages from either the SPI interface or the outbound message queue.
 */
void SpiTransportProvider::ProcessMessages()
{
//    gpio_set_level(Gpio::ESP32_READY_PIN, 1);

    ESP_ERROR_CHECK(spi_slave_queue_trans(VSPI_HOST, _spiTransaction, portMAX_DELAY));
//    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
//    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    //
    //  The Task will get notified that there is something to do in two cases:
    //      - There is at least one message in the send queue.
    //      - SPI transmission/reception has completed.
    //
    //  We must wait until we are woken by one of the events.
    //
    WaitForNotification();
    //
    //  ESP-IDF documentation states that a call to spi_slave_get_trans_result
    //  is mandatory when spi_slave_queue_trans is used.
    //
    ESP_ERROR_CHECK(spi_slave_get_trans_result(VSPI_HOST, &_spiTransaction, portMAX_DELAY));

//    gpio_set_level(Gpio::ESP32_READY_PIN, 0);
    switch (_currentState)
    {
        case Idle:
            ProcessIdleState(_spiTransaction);
            break;
        case AcknowledgingMessageHeader:
            ProcessAcknowledgingMessageHeaderState();
            break;
//        case WaitingForMessageBody:
//            ProcessWaitingForMessageBodyState();
//            break;
//        case AcknowledingMessageBody:
//            ProcessAcknowledingMessageBodyState();
//            break;
//        case WantToSendMessageHeader:
//            ProcessWantToSendMessageHeaderState();
//            break;
//        case WaitingForMessageHeaderAcknowledgement:
//            ProcessWaitingForMessageHeaderAcknowledgementState();
//            break;
//        case WantToSendMessageBody:
//            ProcessWantToSendMessageBodyState();
//            break;
//        case WaitingForMessageBodyAcknowledgement:
//            ProcessWaitingForMessageBodyAcknowledgementState();
//            break;
//        case WantToSendTransportMessage:
//            ProcessWantToSendTransportMessageState();
//            break;
        default:
            ESP_LOGI(__func__, "Un-handled state: %s", SpiStateString(_currentState));
            break;
    }
}

/*
 *      Process any messages for the controller.
 *
 *      The messages will be generated by the incoming SPI interface.
 *
 *      Note that this method is static in order to allow the C methods in
 *      FreeRTOS to call the method.
 *
 *      @param pvParameters
 *          Pointer to the instance of the request handler receiving the
 *          messages for dispatch through the instance specific handler.
 */
void SpiTransportProvider::Task(void *pvParameters)
{
    SpiTransportProvider *sender = (SpiTransportProvider *) pvParameters;

    //
    //  It is important to wait for the outbound message queue to be
    //  created before we tray and process any messages.
    //
    while (!_outboundMessageQueueHandle)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    sender->SetupSpiHeaderTransaction();
    while (true)
    {
        vTaskDelay(50 / portTICK_PERIOD_MS);

        sender->ProcessMessages();
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(sender->_transportProviderTaskHandle);
}

/*
 *      ISR for the GPIO pin indicating that the STM32 is
 *      ready to send data to the ESP32 or that it is ready
 *      to receive data from the ESP32.
 *
 *      The ESP32 ready signal is set to a low state when the
 *      SPI interface has been set up.
 */
void SpiTransportProvider::Stm32IsrHandler(void *pvParameters)
{
}
