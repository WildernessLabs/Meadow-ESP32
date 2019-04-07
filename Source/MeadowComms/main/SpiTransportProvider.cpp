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

/*
 *      Structure for the SPI transaction information.
 */
spi_slave_transaction_t SpiTransportProvider::_transaction;

/*
 *      Handle for the task handler.
 */
TaskHandle_t SpiTransportProvider::_taskHandle = nullptr;


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
    gpio_config_t inputPinConfig;
    inputPinConfig.intr_type = GPIO_INTR_POSEDGE;
    inputPinConfig.pin_bit_mask = (1ULL << Gpio::STM32_READY_PIN);
    inputPinConfig.mode = GPIO_MODE_INPUT;
    inputPinConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    inputPinConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&inputPinConfig);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(Gpio::STM32_READY_PIN, Stm32IsrHandler, this);

    /*
     *  Configure the pull-ups on the SPI pins.
     */
    gpio_set_pull_mode(Gpio::MOSI_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(Gpio::CLOCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(Gpio::CHIP_SELECT_PIN, GPIO_PULLUP_ONLY);

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
    _headerFrameSize = Encoders::CalculateSpiBufferSize(sizeof(Message));
    _headerBuffer = (uint8_t *) heap_caps_malloc(_headerFrameSize, MALLOC_CAP_DMA);
    _transmitBuffer = (uint8_t *) heap_caps_malloc(_headerFrameSize, MALLOC_CAP_DMA);
    _transportFrameBuffer = (uint8_t *) heap_caps_malloc(_headerFrameSize, MALLOC_CAP_DMA);
    memset(_headerBuffer, 0, _headerFrameSize);
    memset(_transmitBuffer, 0, _headerFrameSize);
    memset(_transportFrameBuffer, 0, _headerFrameSize);

    memset(&_transaction, 0, sizeof(_transaction));
    _transaction.rx_buffer = _headerBuffer;
    _transaction.tx_buffer = _transmitBuffer;
    _transaction.length = _headerFrameSize * 8;

    _currentState = Idle;

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

    spi_slave_transaction_t *slaveTransaction;
    ESP_ERROR_CHECK(spi_slave_get_trans_result(VSPI_HOST, &slaveTransaction, portMAX_DELAY));
    if (transaction->length != transaction->trans_len)
    {
        SendErrorResponse(StatusCodes::InvalidHeader);
    }
    else
    {
        uint32_t dataSize = transaction->trans_len >> 3;
        ESP_LOGI(__func__, "Received %d bytes.", dataSize);
        if (transaction->trans_len > 0)
        {
            ESP_LOG_BUFFER_HEX(__func__, transaction->rx_buffer, dataSize);
            memset(transaction->rx_buffer, 0, dataSize);
        }
    }
}

/*
 *      SPI Post setup callback
 *
 *      This method is called when the SPI interface has completed
 *      setup and data is ready for transmission.
 *
 *      @param transaction
 */
void SpiTransportProvider::SpiPostSetupCallback(spi_slave_transaction_t *transaction)
{
    ESP_LOGI(__func__, "SPI Post callback invoked");
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
 *      ISR for the GPIO pin indicating that the STM32 is
 *      ready to send data to the ESP32 or that it is ready
 *      to receive data from the ESP32.
 */
void SpiTransportProvider::Stm32IsrHandler(void *pvParameters)
{
    SpiTransportProvider *sender = (SpiTransportProvider *) pvParameters;
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);
    sender->Echo("STM32 is ready.");
}
