/*
 *  MockTransportProvider.hpp
 *
 *  Mock up of the TransportProvider for unit testing.
 */

#ifndef _MOCK_TRANSPORT_PROVIDER_HPP_
#define _MOCK_TRANSPORT_PROVIDER_HPP_

#include "sdkconfig.h"

#include "driver/gpio.h"

#include <string>

#include "MockMessageDispatcher.hpp"
#include "TransportProviderBase.hpp"

class MockTransportProvider : public TransportProviderBase
{
private:

public:
    /**
     *  @brief Name of this component / task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Default constructor.
     */
    MockTransportProvider();

    /**
     *  @brief Default destructor.
     */
    ~MockTransportProvider();

    /*
     *  @brief Setup the SPI transport class.
     */
    void Setup(MessageDispatcher *messageDispatcher);

    /**
     *  @brief Get the number of messages waiting to be sent to the STM32.
     *
     *  @note TransportProviderBase keeps the outbound queue to itself, so these two
     *        methods publish what it already has.  A test can then see what the
     *        MessageDispatcher handed to the transport without a real SPI peer to
     *        collect it.
     *
     *  @return Number of messages in the outbound queue, -1 if it cannot be read.
     */
    int OutboundMessageQueueLength();

    /**
     *  @brief Get the next message waiting to be sent to the STM32.
     *
     *  @return Pointer to the message, which the caller owns, or nullptr if the queue
     *          is empty.
     */
    Message *GetOutboundMessage();
};

#endif // _MOCK_TRANSPORT_PROVIDER_HPP_
