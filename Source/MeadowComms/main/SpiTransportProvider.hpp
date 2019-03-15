/*
 *      SpiTransportProvider.hpp
 *
 *      Hardware transport provider for messages between the ESP32 and
 *      the STM32 using the SPI interface.
 */

#ifndef _SPI_TRANSPORT_PROVIDER_HPP_
#define _SPI_TRANSPORT_PROVIDER_HPP_

#include "TransportProviderBase.hpp"


class SpiTransportProvider : public TransportProviderBase
{
private:
    /*
     *      Default destructor.
     */
    ~SpiTransportProvider();

public:
    /*
     *      Default constructor.
     */
    SpiTransportProvider();

    /*
     *      Setup the SPI transport class.
     */
    void Setup();
};

#endif /* _SPI_TRANSPORT_PROVIDER_HPP_ */
