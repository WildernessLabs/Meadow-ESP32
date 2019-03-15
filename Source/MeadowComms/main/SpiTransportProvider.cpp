/*
 *      SpiTransportProvider.cpp
 *
 *      Hardware transport provider for messages between the ESP32 and
 *      the STM32 using the SPI interface.
 */

#include "SpiTransportProvider.hpp"


/*
 * ---------------------------------------------------------------------------
 *
 *                         Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */


SpiTransportProvider::SpiTransportProvider() : TransportProviderBase()
{
}

SpiTransportProvider::~SpiTransportProvider()
{
}


/* ---------------------------------------------------------------------------
 *
 *                                  Methods
 *
 * ---------------------------------------------------------------------------
 */

void SpiTransportProvider::Setup()
{
    TransportProviderBase::Setup();
}
