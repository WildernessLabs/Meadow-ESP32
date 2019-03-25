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

#include "Exceptions/MultipleInstancesException.hpp"
#include "TransportProviderBase.hpp"
#include "MessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "Gpio.hpp"

class SpiTransportProvider : public TransportProviderBase
{
private:
    /*
     *      Number of bits in the Message structure (without any payload).
     */
    static const int BITS_PER_HEADER = MESSAGE_SIZE * 8;

    /*
     *      Pointer to the (only) instance of the SpiTransportProvider class.
     *
     *      This object is needed to allow the static callbacks access to the
     *      instance level variables and constants.
     */
    static SpiTransportProvider *_spiInstance;

    /*
     *      SPI Transaction.
     */
    static spi_slave_transaction_t _transaction;

    /*
     *      Buffer to hold a Message header worth of data.
     */
    char *_headerBuffer = nullptr;

    /*
     *      Default destructor.
     */
    ~SpiTransportProvider();

    /*
     *      SPI Post Transmission callback
     *
     *      Note this this method is static as it is used as a callback
     *      in some C code.
     */
    static void SpiPostTransmissionCallback(spi_slave_transaction_t *);

    /*
     *      SPI Post setup callback.
     *
     *      Note this this method is static as it is used as a callback
     *      in some C code.
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
