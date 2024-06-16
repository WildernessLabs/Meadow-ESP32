/*
 *  MockTransportProvider.cpp
 *
 *  Mock up of the TransportProvider for unit testing.
 */

#include "MockTransportProvider.hpp"

/**
 *  @brief Name of this component / task.
 */
const char *MockTransportProvider::COMPONENT_NAME = "MockTransportProvider";

/**
 *  @brief Default constructor for the Mock transport provider.
 */
MockTransportProvider::MockTransportProvider() : TransportProviderBase()
{
}

/**
 *  @brief Default destructor for the Mock transport provider.
 */
MockTransportProvider::~MockTransportProvider()
{
}

/**
 *  @brief Setup the Mock transport provider.
 *
 *  @param messageDispatcher
 *      MessageDispatcher object used to send messages from the SPI
 *      interface through to the interfaces on the ESP32.
 */
void MockTransportProvider::Setup(MessageDispatcher *messageDispatcher)
{
    TransportProviderBase::Setup(messageDispatcher);
}
