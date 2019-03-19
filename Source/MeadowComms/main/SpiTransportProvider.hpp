/*
 *      SpiTransportProvider.hpp
 *
 *      Hardware transport provider for messages between the ESP32 and
 *      the STM32 using the SPI interface.
 */

#ifndef _SPI_TRANSPORT_PROVIDER_HPP_
#define _SPI_TRANSPORT_PROVIDER_HPP_

#include "driver/spi_slave.h"
#include "esp_system.h"

#include "TransportProviderBase.hpp"
#include "MessageDispatcher.hpp"

class SpiTransportProvider : public TransportProviderBase
{
private:
    /*
     *      GPIO pin used to indicate to the STM32 that there is data
     *      waiting for transmission.
     */
    const gpio_num_t HANDSHAKE_PIN = GPIO_NUM_2;

    /*
     *      SPI MISO pin.
     */
    const gpio_num_t MISO_PIN = GPIO_NUM_19;

    /*
     *      SPI MOSI pin.
     */
    const gpio_num_t MOSI_PIN = GPIO_NUM_23;

    /*
     *      SPI Clock pin.
     */
    const gpio_num_t CLOCK_PIN = GPIO_NUM_18;

    /*
     *      SPI Chip Select pin.
     */
    const gpio_num_t CHIP_SELECT_PIN = GPIO_NUM_5;

    /*
     *      Default destructor.
     */
    ~SpiTransportProvider();

    /*
     *      SPI Post Transmission callback
     */
    static void SpiPostTransmissionCallback(spi_slave_transaction_t *);

    /*
     *      SPI Post setup callback.
     */
    static void SpiPostSetupCallback(spi_slave_transaction_t *);

public:
    /*
     *      Default constructor.
     */
    SpiTransportProvider();

    /*
     *      Setup the SPI transport class.
     */
    void Setup(MessageDispatcher *);

    /*
     *      Test method that will simply pass the string through
     *      the MessageDispatcher to verify that the message
     *      passing between layers is working.
     */
    void Echo(const char *);
};

#endif /* _SPI_TRANSPORT_PROVIDER_HPP_ */
