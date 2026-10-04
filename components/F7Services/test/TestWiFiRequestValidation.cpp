/*
 *  TestWiFiRequestValidation.cpp
 *
 *  Tests that the WiFi request handlers check what they are sent, and what they allocate, before
 *  they use it (issue #389).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "unity.h"

#include "Encoders.hpp"
#include "MockMessageDispatcher.hpp"
#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"

#include "TestAssertions.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[WiFiRequestValidation][Emulated]"

/**
 * @brief Gives the tests access to WiFiRequestHandler's request handling.
 */
class TestWiFiRequestValidation
{
public:
    /**
     * @brief Send a request with no payload to the handler and return its response.
     *
     *  The handler is created without starting the radio if none exists.  It answers through
     *  the dispatcher given here for the length of the call, and the dispatcher it had is put
     *  back afterwards, as the handlers share it through a static.
     *
     * @param function Function the request is for.
     *
     * @return The response, which the caller deletes.
     */
    static Message *RequestWithNoPayload(const WiFiFunction::WiFiFunction function)
    {
        NvsManager::Setup();
        MockMessageDispatcher *dispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(dispatcher);

        WiFiRequestHandler *handler = WiFiRequestHandler::_instance;
        if (handler == nullptr)
        {
            handler = new WiFiRequestHandler(dispatcher);
            TEST_ASSERT_NOT_NULL(handler);
        }

        //
        //  Answering sets a bit in the event group, which Setup() creates and these tests do not call.
        //
        const bool madeEventGroup = (WiFiRequestHandler::_xWiFiEventGroup == nullptr);
        if (madeEventGroup)
        {
            WiFiRequestHandler::_xWiFiEventGroup = xEventGroupCreate();
            TEST_ASSERT_NOT_NULL(WiFiRequestHandler::_xWiFiEventGroup);
        }
        IMessageDispatcher * const savedDispatcher = WiFiRequestHandler::_messageDispatcher;
        WiFiRequestHandler::_messageDispatcher = dispatcher;

        Message *request = Message::CreateOnHeap(MessageTypes::Header, Esp32Interfaces::WiFi, function, StatusCodes::Failure, 0, nullptr, 0);
        TEST_ASSERT_NOT_NULL(request);
        if (function == WiFiFunction::SetAntenna)
        {
            handler->SetAntennaRequest(request);
        }
        else
        {
            handler->DisconnectFromAccessPoint(request);
        }

        WiFiRequestHandler::_messageDispatcher = savedDispatcher;
        if (madeEventGroup)
        {
            vEventGroupDelete(WiFiRequestHandler::_xWiFiEventGroup);
            WiFiRequestHandler::_xWiFiEventGroup = nullptr;
        }

        TEST_ASSERT_EQUAL_MESSAGE(1, dispatcher->OutboundMessageQueueLength(), "The handler did not answer.");
        return (dispatcher->GetOutboundMessage());
    }

    /**
     * @brief The most access points a scan reports.
     */
    static uint16_t MaximumAccessPoints()
    {
        return (WiFiRequestHandler::MAXIMUM_ACCESS_POINTS);
    }

    /**
     * @brief Limit a count of access points as a scan does.
     */
    static uint16_t Limit(const uint16_t found)
    {
        return (WiFiRequestHandler::LimitAccessPointCount(found));
    }
};

TEST_CASE("SetAntenna with no payload is refused", CLASS_OF_TESTS)
{
    Message *response = TestWiFiRequestValidation::RequestWithNoPayload(WiFiFunction::SetAntenna);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(StatusCodes::InvalidRequest, response->StatusCode);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    Message::DeleteMessage(response);
}

TEST_CASE("DisconnectFromAccessPoint with no payload is refused", CLASS_OF_TESTS)
{
    Message *response = TestWiFiRequestValidation::RequestWithNoPayload(WiFiFunction::DisconnectFromAccessPoint);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL(StatusCodes::InvalidRequest, response->StatusCode);
    TEST_ASSERT_EQUAL(MessageTypes::Response, response->MessageType);
    Message::DeleteMessage(response);
}

TEST_CASE("A scan reports no more access points than fit in one message", CLASS_OF_TESTS)
{
    const uint16_t maximum = TestWiFiRequestValidation::MaximumAccessPoints();

    TEST_ASSERT_EQUAL(0, TestWiFiRequestValidation::Limit(0));
    TEST_ASSERT_EQUAL(1, TestWiFiRequestValidation::Limit(1));
    TEST_ASSERT_EQUAL(maximum, TestWiFiRequestValidation::Limit(maximum));
    TEST_ASSERT_EQUAL_MESSAGE(maximum, TestWiFiRequestValidation::Limit(maximum + 1), "A scan reported more access points than the limit.");
    TEST_ASSERT_EQUAL(maximum, TestWiFiRequestValidation::Limit(UINT16_MAX));

    //
    //  The limit is the largest count whose list fits in a message: the list is an 8 byte header and
    //  then one record for each access point.
    //
    Esp32Messaging::AccessPointList list = {};
    Esp32Messaging::AccessPoint accessPoint = {};
    const uint32_t recordSize = Encoders::EncodedAccessPointBufferSize(&accessPoint);

    list.AccessPointsLength = recordSize * maximum;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(Message::MAXIMUM_PAYLOAD_LENGTH, Encoders::EncodedAccessPointListBufferSize(&list));
    list.AccessPointsLength = recordSize * (maximum + 1);
    TEST_ASSERT_GREATER_THAN_UINT32(Message::MAXIMUM_PAYLOAD_LENGTH, Encoders::EncodedAccessPointListBufferSize(&list));
}
