/*
 *      SpiTransportProvider.cpp
 *
 *      Hardware transport provider for messages between the ESP32 and
 *      the STM32 using the SPI interface.
 */

#include "SpiTransportProvider.hpp"
#include "Esp32Messaging.hpp"

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

void SpiTransportProvider::Echo(char *messageString)
{
    Esp32Messaging::Message message;

    message.Interface = Esp32Interfaces::System;
    message.Function = SystemFunction::Echo;
}
