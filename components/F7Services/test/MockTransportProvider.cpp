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

/**
 *  @brief Get the number of messages waiting to be sent to the STM32.
 *
 *  @return Number of messages in the outbound queue, -1 if it cannot be read.
 */
int MockTransportProvider::OutboundMessageQueueLength()
{
    return (TransportProviderBase::OutboundMessageQueueLength());
}

/**
 *  @brief Get the next message waiting to be sent to the STM32.
 *
 *  @return Pointer to the message, which the caller owns, or nullptr if the queue is empty.
 */
Message *MockTransportProvider::GetOutboundMessage()
{
    return (TransportProviderBase::GetOutboundMessage());
}
