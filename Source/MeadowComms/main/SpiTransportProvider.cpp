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
//    chipSelectConfiguration.pull_up_en = GPIO_PULLUP_ENABLE;
//    chipSelectConfiguration.pull_down_en = GPIO_PULLDOWN_DISABLE;
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
 *
 */
//void SpiTransportProvider::AcknowledgeMessage(SpiStates newState, StatusCodes::StatusCodes code, PacketType packetType)
//{
//    Message dummyMessage;
//
//    memset(&dummyMessage, 0, sizeof(Message));
//    dummyMessage.MessageType = packetType;
//    dummyMessage.Interface = Esp32Interfaces::Transport;
//    dummyMessage.Function = 0;
//    dummyMessage.MessageID = 0;
//    dummyMessage.StatusCode = code;
//    dummyMessage.Payload = nullptr;
//    dummyMessage.PayloadLength = 0;
//    AcknowledgeMessage(&dummyMessage, newState, code, packetType);
//}

/*
 *      Start the acknowledgement process.
 */
spi_slave_transaction_t *SpiTransportProvider::AcknowledgeMessage(Message *message, StatusCodes::StatusCodes code, PacketType packetType)
{
    spi_slave_transaction_t *transaction = (spi_slave_transaction_t *) pvPortMalloc(sizeof(spi_slave_transaction_t));

    memset(transaction, 0, sizeof(spi_slave_transaction_t));
    uint8_t *transmitBuffer = (uint8_t *) pvPortMalloc(_headerFrameSize);
    uint8_t *receiveBuffer = (uint8_t *) pvPortMalloc(_headerFrameSize);

    _currentHeader->MessageType = (uint8_t) packetType;
    _currentHeader->StatusCode = (uint32_t) code;
    uint8_t *ackFrame = (uint8_t *) Encoders::EncodeMessage(message);
    memset(transmitBuffer, 0, _headerFrameSize);
    memcpy(transmitBuffer, ackFrame, _headerFrameSize);
    vPortFree(ackFrame);

    memset(receiveBuffer, 0, _headerFrameSize);

    transaction->rx_buffer = receiveBuffer;
    transaction->tx_buffer = transmitBuffer;       // This will have been setup by the Acknowledge functions.
    transaction->length = _headerFrameSize * 8;

    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    return(transaction);
}

///*
// *
// */
//void SpiTransportProvider::SendErrorResponse(StatusCodes::StatusCodes errorCode)
//{
//    Message *message;
//
//    message = (Message *) _transmitBuffer;
//    message->Interface = Esp32Interfaces::System;
//    message->Function = SystemFunction::Echo;
//    message->MessageID = 0x00;
//    message->MessageType = PacketType::Data;
//    message->PayloadLength = 0;
//    message->Payload = (uint8_t *) 0;
//}

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


void SpiTransportProvider::ClearCurrentHeader()
{
    if (_currentHeader)
    {
        vPortFree(_currentHeader);
        _currentHeader = nullptr;
    }
}

/*
 *
 */
void SpiTransportProvider::ProcessIdleState()
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t transaction;

    memset(&transaction, 0, sizeof(transaction));
    uint8_t *transmitBuffer = (uint8_t *) pvPortMalloc(_headerFrameSize);
    uint8_t *receiveBuffer = (uint8_t *) pvPortMalloc(_headerFrameSize);
    memset(receiveBuffer, 0, _headerFrameSize);
    memset(transmitBuffer, 0, _headerFrameSize);

    transaction.rx_buffer = receiveBuffer;
    transaction.tx_buffer = transmitBuffer;
    transaction.length = _headerFrameSize * 8;

    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, &transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    int dataSize = transaction.length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction.rx_buffer, dataSize);

    Message *messageHeader;
    if (transaction.trans_len >= (HEADER_SIZE * 8))
    {
        messageHeader = Encoders::ExtractMessage((uint8_t *) transaction.rx_buffer, _headerFrameSize);
        //
        //  TODO: message could be nullptr - deal with it.
        //
        StatusCodes::StatusCodes code;
        PacketType type;
        ClearCurrentHeader();
        if (messageHeader)
        {
            _responseCode = StatusCodes::CompletedOK;
            _packetType = PacketType::Ack;
        }
        else
        {
            _responseCode = StatusCodes::CRCError;
            _packetType = PacketType::Nak;

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
        _packetType = PacketType::Nak;

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

    vPortFree(receiveBuffer);
    vPortFree(transmitBuffer);
    ClearCurrentHeader();
    _currentHeader = messageHeader;
    _currentState = AcknowledgingMessageHeader;
}

void SpiTransportProvider::ProcessWaitingForMessageHeaderState()
{
    ESP_LOGI(__func__, "Entering Method.");
}

void SpiTransportProvider::ProcessWaitingForHeaderAckRequestState()
{
    ESP_LOGI(__func__, "Entering Method.");
}

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
    vPortFree((void *) transaction->rx_buffer);
    vPortFree((void *) transaction->tx_buffer);
    vPortFree(transaction);
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

void SpiTransportProvider::ProcessWaitingForMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t transaction;

    memset(&transaction, 0, sizeof(transaction));

    uint32_t encodedMessageBodySize = Encoders::CalculateSpiBufferSize(HEADER_SIZE + _currentHeader->PayloadLength);
    ESP_LOGI(__func__, "Current header payload length: %d", _currentHeader->PayloadLength);
    ESP_LOGI(__func__, "Message body buffer size: %d", encodedMessageBodySize);

    uint8_t *encodedMessageBody = (uint8_t *) pvPortMalloc(encodedMessageBodySize);
    memset(encodedMessageBody, 0, encodedMessageBodySize);

    uint8_t *dummyTxBuffer = (uint8_t *) pvPortMalloc(encodedMessageBodySize);
    memset(dummyTxBuffer, 0, encodedMessageBodySize);

    transaction.rx_buffer = encodedMessageBody;
    transaction.tx_buffer = dummyTxBuffer;
    transaction.length = encodedMessageBodySize * 8;

    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, &transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    int dataSize = transaction.length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction.rx_buffer, dataSize);
    Message *message = Encoders::ExtractMessage((uint8_t *) transaction.rx_buffer, encodedMessageBodySize);

    StatusCodes::StatusCodes code;
    PacketType type;
    if (message)
    {
        code = StatusCodes::CompletedOK;
        type = PacketType::Ack;
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
        code = StatusCodes::CRCError;
        type = PacketType::Nak;
        ClearCurrentHeader();
    }

    vPortFree(encodedMessageBody);
    vPortFree(dummyTxBuffer);
    if (_currentHeader)
    {
        ClearCurrentHeader();
        _currentState = AcknowledingMessageBody;
    }
    else
    {
        _currentState = Idle;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

void SpiTransportProvider::ProcessAcknowledingMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");

    spi_slave_transaction_t transaction;

    memset(&transaction, 0, sizeof(transaction));
    memset(_headerBuffer, 0, _headerFrameSize);

    transaction.rx_buffer = _headerBuffer;
    transaction.tx_buffer = _transmitBuffer;       // This will have been setup by the Acknowledge functions.
    transaction.length = _headerFrameSize * 8;

    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, &transaction, portMAX_DELAY));
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);

    int dataSize = transaction.length / 8;
    ESP_LOGI(__func__, "Received %d bytes.", dataSize);
    ESP_LOG_BUFFER_HEX(__func__, transaction.rx_buffer, dataSize);

    ClearCurrentHeader();
    _currentState = Idle;
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

void SpiTransportProvider::ProcessWantToSendMessageHeaderState()
{
    ESP_LOGI(__func__, "Entering Method.");
}

void SpiTransportProvider::ProcessWantToSendMessageBodyState()
{
    ESP_LOGI(__func__, "Entering Method.");
}

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
            case WaitingForMessageHeader:
                sender->ProcessWaitingForMessageHeaderState();
                break;
            case AcknowledgingMessageHeader:
                sender->ProcessAcknowledgingMessageHeaderState();
                break;
            case WaitingForHeaderAckRequest:
                sender->ProcessWaitingForHeaderAckRequestState();
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
