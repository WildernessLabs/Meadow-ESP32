/*
 *      SpiTransportProvider.cpp
 *
 *      Hardware transport provider for messages between the ESP32 and
 *      the STM32 using the SPI interface.
 */
#include "freertos/FreeRTOS.h"
#include "SpiTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include <string.h>

/*
 * ----------------------------------------------------------------------------
 *
 *                   Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */
SpiTransportProvider *SpiTransportProvider::_spiInstance = nullptr;

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
    //
    //  Should only get here if this is the first instance.
    //
    _spiInstance = this;

    /*
     *  Set the handshake pin to output and set the level low.
     */
    gpio_set_direction(Gpio::BOOT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(Gpio::BOOT_PIN, 0);

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
    spiBusConfiguration.mosi_io_num = Gpio::MOSI_PIN;
    spiBusConfiguration.miso_io_num = Gpio::MISO_PIN;
    spiBusConfiguration.sclk_io_num = Gpio::CLOCK_PIN;

    /*
     *  Configuration for the SPI slave interface.
     */
    spi_slave_interface_config_t slaveConfiguration;
    slaveConfiguration.mode = 0;
    slaveConfiguration.spics_io_num = Gpio::CHIP_SELECT_PIN;
    slaveConfiguration.queue_size = 3;
    slaveConfiguration.flags = 0;
    slaveConfiguration.post_setup_cb = SpiPostSetupCallback;
    slaveConfiguration.post_trans_cb = SpiPostTransmissionCallback;

    ESP_ERROR_CHECK(spi_slave_initialize(VSPI_HOST, &spiBusConfiguration, &slaveConfiguration, 1));

    _headerBuffer = (char *) heap_caps_malloc(BITS_PER_HEADER, MALLOC_CAP_DMA);
}

/*
 *      Default destructor for the SPI transport provider.
 */
SpiTransportProvider::~SpiTransportProvider()
{
}

/* ---------------------------------------------------------------------------
 *
 *                                  Methods
 *
 * ---------------------------------------------------------------------------
 */

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
    gpio_set_level(Gpio::BOOT_PIN, 0);
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
    gpio_set_level(Gpio::BOOT_PIN, 1);
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
    message->Crc = 0x00;
    _messageDispatcher->QueueMessageForEsp32(message);
}
