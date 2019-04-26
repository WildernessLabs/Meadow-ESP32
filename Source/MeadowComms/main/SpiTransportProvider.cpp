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
uint8_t *SpiTransportProvider::_headerBuffer = nullptr;
uint8_t *SpiTransportProvider::_transmitBuffer = nullptr;
uint8_t *SpiTransportProvider::_transportFrameBuffer = nullptr;
uint32_t SpiTransportProvider::_headerFrameSize = 0;
QueueHandle_t SpiTransportProvider::_incomingMessageQueue = nullptr;

/*
 *      Structure for the SPI transaction information.
 */
//spi_slave_transaction_t SpiTransportProvider::_transaction;

/*
 *      Handle for the task handler.
 */
TaskHandle_t SpiTransportProvider::_taskHandle = nullptr;
TaskHandle_t SpiTransportProvider::_spiMessageTaskHandle = nullptr;


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
     *  Set the handshake pins to output and set the level low for
     *  the output pin and register the ISR for the input pin.
     */
    gpio_set_direction(Gpio::ESP32_READY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    /*
     * Configure the STM32 Ready signal pin for interrupts.
     *
     * Interrupt on rising edges on the STM32_READY_PIN.  The pin has
     * pull-up disabled and pull-downs enabled.
     *
     */
    gpio_config_t stm32ReadyPinConfig;
    stm32ReadyPinConfig.intr_type = GPIO_INTR_POSEDGE;
    stm32ReadyPinConfig.pin_bit_mask = (1ULL << Gpio::STM32_READY_PIN);
    stm32ReadyPinConfig.mode = GPIO_MODE_INPUT;
    stm32ReadyPinConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    stm32ReadyPinConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&stm32ReadyPinConfig));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(Gpio::STM32_READY_PIN, Stm32IsrHandler, this));

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
     */
    _headerFrameSize = Encoders::CalculateSpiBufferSize(HEADER_SIZE);
    _headerBuffer = (uint8_t *) heap_caps_malloc(_headerFrameSize, MALLOC_CAP_DMA);
    _transmitBuffer = (uint8_t *) heap_caps_malloc(_headerFrameSize, MALLOC_CAP_DMA);
    _transportFrameBuffer = (uint8_t *) heap_caps_malloc(_headerFrameSize, MALLOC_CAP_DMA);
    memset(_headerBuffer, 0, _headerFrameSize);
    memset(_transmitBuffer, 0, _headerFrameSize);
    memset(_transportFrameBuffer, 0, _headerFrameSize);

    _currentState = Idle;
    _currentStateMutexHandle = xSemaphoreCreateMutex();
    if (_currentStateMutexHandle == NULL)
    {
        ESP_LOGE(__func__, "Cannot create mutex for the current state variable.");
    }

    /*
     *  Set up the task and message queue for the incoming SPI messages.
     *
     *  Set up the message queue first as the task will wait on the queue.
     */
    _incomingMessageQueue = xQueueCreate(1, sizeof(spi_slave_transaction_t));
    if (_incomingMessageQueue == NULL)
    {
        ESP_LOGE(__func__, "Cannot allocate SPI incoming message queue.");
    }
    xTaskCreate(IncomingSpiMessageTask, "IncomingSPIMessageTask", 2048 * 2, this, configMAX_PRIORITIES - 1, &SpiTransportProvider::_spiMessageTaskHandle);

    //
    //  Indicate to the STM32 that we are ready to receive requests.
    //
    gpio_set_level(Gpio::ESP32_READY_PIN, 1);
}

/*
 *      Default destructor for the SPI transport provider.
 */
SpiTransportProvider::~SpiTransportProvider()
{
    spi_slave_free(VSPI_HOST);
    if (_headerBuffer)
    {
        vPortFree(_headerBuffer);
    }
    if (_transmitBuffer)
    {
        vPortFree(_transmitBuffer);
    }
}

/* ---------------------------------------------------------------------------
 *
 *                                  Methods
 *
 * ---------------------------------------------------------------------------
 */


/*
 *      Start the acknowledgement process.
 *
 *      @param message
 *          Message to acknowledge.
 *
 *      @param code
 *          Status code to return.
 *
 *      @param packetType
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
spi_slave_transaction_t *SpiTransportProvider::AcknowledgeMessage(Message *message, StatusCodes::StatusCodes code, uint8_t packetType)
{
    spi_slave_transaction_t *transaction = SetupSpiSlaveTransaction(_headerFrameSize, _headerFrameSize);

    Message *copyOfMessage = CopyMessage(message, false);
    copyOfMessage->MessageType = packetType;
    copyOfMessage->StatusCode = (uint32_t) code;
    uint8_t *ackFrame = (uint8_t *) Encoders::EncodeMessage(copyOfMessage);
    vPortFree(copyOfMessage);
    memcpy((void *) transaction->tx_buffer, ackFrame, _headerFrameSize);
    vPortFree(ackFrame);

    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    return(transaction);
}

/*
 *      Create a new SPI slave transmission object with zero filled
 *      buffers of the requested size.
 *
 *      @param receiveBufferSize
 *          Number of bytes in the receive buffer.
 *
 *      @param transmitBufferSize
 *          Number of bytes in the transmit buffer.
 *
 *      @returns
 *          Pointer to a spi_slave_transaction_t object with the
 *          buffers set up and filled with zeroes.
 */
spi_slave_transaction_t * SpiTransportProvider::SetupSpiSlaveTransaction(uint32_t receiveBufferSize, uint32_t transmitBufferSize)
{
    spi_slave_transaction_t *transaction = (spi_slave_transaction_t *) pvPortMalloc(sizeof(spi_slave_transaction_t));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));
    uint8_t *transmitBuffer = (uint8_t *) heap_caps_malloc(transmitBufferSize, MALLOC_CAP_DMA);
    uint8_t *receiveBuffer = (uint8_t *) heap_caps_malloc(receiveBufferSize, MALLOC_CAP_DMA);
    memset(receiveBuffer, 0, receiveBufferSize);
    memset(transmitBuffer, 0, transmitBufferSize);

    transaction->rx_buffer = receiveBuffer;
    transaction->tx_buffer = transmitBuffer;
    transaction->length = transmitBufferSize * 8;

    return(transaction);
}

/*
 *      Release any storage allocated on the heap associated
 *      with an SPI transaction.
 *
 *      @param transaction
 *          Pointer to the transaction (on the heap)
 */
void SpiTransportProvider::FreeUpTransactionHeapStorage(spi_slave_transaction_t *transaction)
{
    heap_caps_free((void *) transaction->rx_buffer);
    heap_caps_free((void *) transaction->tx_buffer);
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
    message->MessageType = PacketType::Data;
    message->PayloadLength = strlen(messageString) + 1;
    char *str = (char *) pvPortMalloc(message->PayloadLength);
    strcpy(str, messageString);
    message->Payload = (uint8_t *) str;
    _messageDispatcher->QueueMessageForEsp32(message);
}

/*
 *      Process any messages for the controller.
 *
 *      The messages will be generated by the incoming UART task.
 *
 *      @param pvParameters
 *          Pointer to the instance of the request handler receiving the
 *          messages for dispatch through the instance specific handler.
 */
void SpiTransportProvider::Task(void *pvParameters)
{
    SpiTransportProvider *sender = (SpiTransportProvider *) pvParameters;
    while (true)
    {
        Message *message;
        BaseType_t result = xQueueReceive(sender->_queueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            //sender->DispatchRequest(message);
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(sender->_taskHandle);
}

/*
 *      Clear the class level variable _currentHeader.
 *
 *      This method ensures that any heap storage is released and the
 *      pointer is set to the nullptr value.
 */
void SpiTransportProvider::ClearCurrentHeader()
{
    if (_currentHeader)
    {
        vPortFree(_currentHeader);
        _currentHeader = nullptr;
    }
}

/*
 *      The STM32 has indicated that it has a message for the ESP32.
 *
 *      This method requests header information from the STM32 and starts
 *      the request process.
 */
void SpiTransportProvider::ProcessIdleState()
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t *transaction = SetupSpiSlaveTransaction(_headerFrameSize, _headerFrameSize);

    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);

    Message *messageHeader;
    if (transaction->trans_len >= (HEADER_SIZE * 8))
    {
        messageHeader = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, _headerFrameSize);
        //
        //  TODO: message could be nullptr - deal with it.
        //
        ClearCurrentHeader();
        if (messageHeader)
        {
            _responseCode = StatusCodes::CompletedOK;
            _packetType = PacketType::Transport | PacketType::Ack;
        }
        else
        {
            _responseCode = StatusCodes::CRCError;
            _packetType = PacketType::Transport | PacketType::Nak;

            messageHeader = (Message *) pvPortMalloc(sizeof(Message));
            memset(messageHeader, 0, sizeof(Message));
            messageHeader->MessageType = _packetType;
            messageHeader->Interface = Esp32Interfaces::Transport;
            messageHeader->Function = 0;
            messageHeader->MessageID = 0;
            messageHeader->StatusCode = _responseCode;
            messageHeader->Payload = nullptr;
            messageHeader->PayloadLength = 0;
        }
    }
    else
    {
        _responseCode = StatusCodes::InvalidHeader;
        _packetType = PacketType::Transport | PacketType::Nak;

        messageHeader = (Message *) pvPortMalloc(sizeof(Message));
        memset(messageHeader, 0, sizeof(Message));
        messageHeader->MessageType = _packetType;
        messageHeader->Interface = Esp32Interfaces::Transport;
        messageHeader->Function = 0;
        messageHeader->MessageID = 0;
        messageHeader->StatusCode = _responseCode;
        messageHeader->Payload = nullptr;
        messageHeader->PayloadLength = 0;
    }
    FreeUpTransactionHeapStorage(transaction);
    ClearCurrentHeader();
    _currentHeader = messageHeader;
    _currentState = AcknowledgingMessageHeader;
}

/*
 *      Acknowledge the message header that has been received and work out the next steps.
 */
void SpiTransportProvider::ProcessAcknowledgingMessageHeaderState()
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t *transaction = AcknowledgeMessage(_currentHeader, _responseCode, _packetType);
    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);

    if (_currentHeader)
    {
        if (_currentHeader->PayloadLength == 0)
        {
            //
            //  If we get here then the message is simple, i.e. no payload
            //  so we need to queue the message on the appropriate interface.
            //
            if (_currentHeader->Interface != Esp32Interfaces::Transport)
            {
                _messageDispatcher->QueueMessageForEsp32(_currentHeader);
            }
            //
            //
            //
            ClearCurrentHeader();
            _currentState = Idle;
        }
        else
        {
            //
            //  Complex payload, we need to ask for the full message including
            //  the message payload.
            //
            _currentState = WaitingForMessageBody;
        }
    }
    else
    {
        _currentState = Idle;
    }
    FreeUpTransactionHeapStorage(transaction);
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

/*
 *      Previous messages have indicated that a message body is required, request the message body.
 */
void SpiTransportProvider::ProcessWaitingForMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");

    uint32_t encodedMessageBodySize = Encoders::CalculateSpiBufferSize(HEADER_SIZE + _currentHeader->PayloadLength);
    spi_slave_transaction_t *transaction = SetupSpiSlaveTransaction(encodedMessageBodySize, encodedMessageBodySize);

    ESP_LOGI(__func__, "Current header payload length: %d", _currentHeader->PayloadLength);
    ESP_LOGI(__func__, "Message body buffer size: %d", encodedMessageBodySize);

    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
    Message *message = Encoders::ExtractMessage((uint8_t *) transaction->rx_buffer, encodedMessageBodySize);

    if (message)
    {
        _responseCode = StatusCodes::CompletedOK;
        _packetType = PacketType::Transport | PacketType::Ack;
        if (_currentHeader->Interface != Esp32Interfaces::Transport)
        {
            _messageDispatcher->QueueMessageForEsp32(message);
        }
        else
        {
            ClearCurrentHeader();
        }
    }
    else
    {
        _responseCode = StatusCodes::CRCError;
        _packetType = PacketType::Transport | PacketType::Nak;
        ClearCurrentHeader();
    }
    FreeUpTransactionHeapStorage(transaction);
    if (_currentHeader)
    {
        _currentState = AcknowledingMessageBody;
    }
    else
    {
        _currentState = Idle;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

/*
 *      Acknowledge the receipt of the message body.
 *
 *      This method requires the following class level variables to have
 *      been setup correctly:
 *
 *          _currentHeader
 *          _responseCode
 *          _packetType
 */
void SpiTransportProvider::ProcessAcknowledingMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t *transaction = AcknowledgeMessage(_currentHeader, _responseCode, _packetType);
    int dataSize = transaction->length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);

    FreeUpTransactionHeapStorage(transaction);
    ClearCurrentHeader();
    _currentState = Idle;
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

/*
 *      Send a message header to the STM32.
 */
void SpiTransportProvider::ProcessWantToSendMessageHeaderState()
{
    ESP_LOGI(__func__, "Entering Method.");
}

/*
 *      Send a message body to the STM32.
 */
void SpiTransportProvider::ProcessWantToSendMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");
}

/*
 *      Start the transport message transmission process.
 */
void SpiTransportProvider::ProcessWantToSendTransportMessageState()
{
    ESP_LOGI(__func__, "Entering Method.");
}

/*
 *      Process any messages for the controller.
 *
 *      The messages will be generated by the incoming UART task.
 *
 *      @param pvParameters
 *          Pointer to the instance of the request handler receiving the
 *          messages for dispatch through the instance specific handler.
 */
void SpiTransportProvider::IncomingSpiMessageTask(void *pvParameters)
{
    SpiTransportProvider *sender = (SpiTransportProvider *) pvParameters;

    while (true)
    {
        //
        //  The Task will get notified that there is something to do in two cases:
        //      - STM32 interrupt (the STM32 wants us to talk to it)
        //      - There is at least one message in the send queue.
        //      - SPI transmission/reception has completed.
        //
        //  We must wait until we are woken by one of the events.
        //
        ESP_LOGI(__func__, "Waiting for notification.");
        ESP_LOGI(__func__, "Notification Received, current state: %d", _currentState);
        gpio_set_level(Gpio::ESP32_READY_PIN, 1);
        uint32_t notificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        switch (_currentState)
        {
            case Idle:
                sender->ProcessIdleState();
                break;
            case AcknowledgingMessageHeader:
                sender->ProcessAcknowledgingMessageHeaderState();
                break;
            case WaitingForMessageBody:
                sender->ProcessWaitingForMessageBodyState();
                break;
            case AcknowledingMessageBody:
                sender->ProcessAcknowledingMessageBodyState();
                break;
            case WantToSendMessageHeader:
                sender->ProcessWantToSendMessageHeaderState();
                break;
            case WantToSendMessageBody:
                sender->ProcessWantToSendMessageBodyState();
                break;
            case WantToSendTransportMessage:
                sender->ProcessWantToSendTransportMessageState();
                break;
            default:
                break;
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(sender->_spiMessageTaskHandle);
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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(_spiMessageTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR();
}
