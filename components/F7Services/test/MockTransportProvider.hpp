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
};

#endif // _MOCK_TRANSPORT_PROVIDER_HPP_
