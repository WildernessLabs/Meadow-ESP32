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
spi_slave_transaction_t SpiTransportProvider::_transaction;

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

void SpiTransportProvider::SendErrorResponse(StatusCodes::StatusCodes errorCode)
{
    Message *message;

    message = (Message *) _transmitBuffer;
    message->Interface = Esp32Interfaces::System;
    message->Function = SystemFunction::Echo;
    message->MessageID = 0x00;
    message->MessageType = TransportPacketType::Data;
    message->PayloadLength = 0;
    message->Payload = (uint8_t *) 0;
}

/*
 *      SPI Post Transmission callback
 *
 *      This method is called when the SPI interface has received
 *      the full data load from the STM32.
 *
 *      @param transaction
 */
void SpiTransportProvider::SpiPostTransmissionCallback(spi_slave_transaction_t *transaction)
{
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);
    //
    //  The ESP32 API reference states that spi_slave_get_trans_result must be called when
    //  using the queueing method we are using.
    //
    spi_slave_transaction_t *slaveTransaction;
    spi_slave_get_trans_result(VSPI_HOST, &slaveTransaction, (5 / portTICK_PERIOD_MS));
    //
    // TODO: Error checking on queueing SPI message from ISR.
    //
    xQueueSendToBackFromISR(SpiTransportProvider::_incomingMessageQueue, transaction , NULL);
}

/*
 *      SPI Post setup callback
 *
 *      This method is called when the SPI interface has completed
 *      setup and data is ready for transmission.
 *
 *      This method sets the ESP32 Ready pin to high to indicate to the STM32
 *      that we are ready to send / receive data.
 *
 *      @param transaction
 */
void SpiTransportProvider::SpiPostSetupCallback(spi_slave_transaction_t *transaction)
{
    gpio_set_level(Gpio::ESP32_READY_PIN, 1);
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
    message->MessageType = TransportPacketType::Data;
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
        spi_slave_transaction_t spiTransaction;
        BaseType_t result = xQueueReceive(sender->_incomingMessageQueue, &spiTransaction, portMAX_DELAY);
        if (result == pdTRUE)
        {
            ESP_LOGI(__func__, "Received message from queue.");
            if (spiTransaction.trans_len > 0)
            {
                int dataSize = spiTransaction.length / 8;
                ESP_LOGI(__func__, "Received %d bytes.", dataSize);
                ESP_LOG_BUFFER_HEX(__func__, spiTransaction.rx_buffer, dataSize);
                Message *message = Encoders::ExtractMessage((uint8_t *) spiTransaction.rx_buffer, _headerFrameSize);
                if (message)
                {
                    ESP_LOGI(__func__, "Interface: %d", message->Interface);
                    ESP_LOGI(__func__, "Function: %d", message->Function);
                    ESP_LOGI(__func__, "Message ID: %d", message->MessageID);
                    ESP_LOGI(__func__, "Message type: %d", message->MessageType);
                }
                memset(spiTransaction.rx_buffer, 0, dataSize);
            }
            else
            {
                ESP_LOGI(__func__, "Message is too short.");
            }
        }
        _currentState = Idle;
        gpio_set_level(Gpio::ESP32_READY_PIN, 1);
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(sender->_spiMessageTaskHandle);
}

void SpiTransportProvider::WaitForData()
{
    memset(_transaction.rx_buffer, 0, 23);
    ESP_ERROR_CHECK(spi_slave_transmit(VSPI_HOST, &_transaction, portMAX_DELAY));
    ESP_LOGI(__func__, "Received %d bytes from STM32", _transaction.trans_len / 8);
    ESP_LOG_BUFFER_HEXDUMP(__func__, _transaction.rx_buffer, 23, ESP_LOG_INFO);

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
    SpiTransportProvider *sender = (SpiTransportProvider *) pvParameters;
    switch (_currentState)
    {
        case Idle:
            memset(&_transaction, 0, sizeof(_transaction));
            _transaction.rx_buffer = _headerBuffer;
            _transaction.tx_buffer = _transmitBuffer;
            _transaction.length = _headerFrameSize * 8;
            _currentState = WaitingForMessageHeader;
            ESP_ERROR_CHECK(spi_slave_queue_trans(VSPI_HOST, &_transaction, 0));
            break;
        case WaitingForMessageHeader:
            break;
        case WaitingForMessageBody:
            break;
        case WantToSendMessageHeader:
            break;
        case WantToSendMessageBody:
            break;
        case WantToSendTransportMessage:
            break;
        default:
            //  TODO: Deal with invalid SPI state.
            break;
    }
}
