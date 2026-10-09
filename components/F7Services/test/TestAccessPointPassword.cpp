/*
 *  TestAccessPointPassword.cpp
 *
 *  Tests that a soft-AP password the soft-AP cannot be started with is refused as
 *  InvalidWiFiCredentials before the interface is touched (issue #537).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "unity.h"

#include "Encoders.hpp"
#include "MockMessageDispatcher.hpp"
#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"

#include "TestAssertions.hpp"
#include "TestWiFiHelpers.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[AccessPointPassword][Emulated]"

/**
 * @brief Longest password a test builds, with its terminating NUL.
 */
#define PASSWORD_BUFFER_SIZE 80

/**
 * @brief Gives the tests access to WiFiRequestHandler's soft-AP password checks.
 */
class TestAccessPointPassword
{
public:
    /**
     * @brief Is the password one the soft-AP can be started with?
     *
     * @param password Password to check.
     *
     * @return true if it is.
     */
    static bool IsValid(const char *password)
    {
        return (WiFiRequestHandler::IsValidAccessPointPassword(password));
    }

    /**
     * @brief Get the handler, making one that does not start the radio if there is none.
     *
     * @param dispatcher Dispatcher a new handler is given.
     *
     * @return The handler.
     */
    static WiFiRequestHandler *Handler(MockMessageDispatcher *dispatcher)
    {
        NvsManager::Setup();
        WiFiRequestHandler *handler = WiFiRequestHandler::_instance;
        if (handler == nullptr)
        {
            handler = new WiFiRequestHandler(dispatcher);
            TEST_ASSERT_NOT_NULL(handler);
        }
        return (handler);
    }

    /**
     * @brief Run a soft-AP request's information through the validation.
     *
     * @param ssid Name of the soft-AP.
     * @param password Password of the soft-AP.
     * @param status Set to the reason the request was refused.
     *
     * @return true if the request was accepted.
     */
    static bool Accepts(const char *ssid, const char *password, StatusCodes::StatusCodes &status)
    {
        MockMessageDispatcher *dispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(dispatcher);
        WiFiRequestHandler *handler = Handler(dispatcher);

        Message *request = CreateAccessPointMessage(WiFiFunction::StartAccessPoint, ssid, password);
        Esp32Messaging::AccessPointInformation *information = handler->ExtractAndValidateAccessPointInformation(request->Payload, request->PayloadLength, status);
        const bool accepted = (information != nullptr);
        Encoders::FreeAccessPointInformation(&information);
        Message::DeleteMessage(request);
        return (accepted);
    }

    /**
     * @brief Send a StartAccessPoint request and return the status it was answered with.
     *
     *  Answering sets a bit in the event group, which Setup() creates and these tests do not
     *  call.  The request is only ever one that is refused, so the radio is never started.
     *
     * @param ssid Name of the soft-AP.
     * @param password Password of the soft-AP.
     * @param interfaceTouched Set to true if the request started the WiFi interface or the soft-AP.
     *
     * @return The status of the response.
     */
    static StatusCodes::StatusCodes Start(const char *ssid, const char *password, bool &interfaceTouched)
    {
        MockMessageDispatcher *dispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(dispatcher);
        WiFiRequestHandler *handler = Handler(dispatcher);

        const bool madeEventGroup = (WiFiRequestHandler::_xWiFiEventGroup == nullptr);
        if (madeEventGroup)
        {
            WiFiRequestHandler::_xWiFiEventGroup = xEventGroupCreate();
            TEST_ASSERT_NOT_NULL(WiFiRequestHandler::_xWiFiEventGroup);
        }
        IMessageDispatcher * const savedDispatcher = WiFiRequestHandler::_messageDispatcher;
        WiFiRequestHandler::_messageDispatcher = dispatcher;

        Message *request = CreateAccessPointMessage(WiFiFunction::StartAccessPoint, ssid, password);
        handler->StartAccessPoint(request);

        const EventBits_t bits = xEventGroupGetBits(WiFiRequestHandler::_xWiFiEventGroup);
        interfaceTouched = ((bits & (WiFiRequestHandler::WIFI_INTERFACE_STARTED_BIT | WiFiRequestHandler::WIFI_ACCESS_POINT_STARTED_BIT)) != 0);

        WiFiRequestHandler::_messageDispatcher = savedDispatcher;
        if (madeEventGroup)
        {
            vEventGroupDelete(WiFiRequestHandler::_xWiFiEventGroup);
            WiFiRequestHandler::_xWiFiEventGroup = nullptr;
        }

        TEST_ASSERT_EQUAL_MESSAGE(1, dispatcher->OutboundMessageQueueLength(), "The handler did not answer.");
        Message *response = dispatcher->GetOutboundMessage();
        TEST_ASSERT_NOT_NULL(response);
        const StatusCodes::StatusCodes status = static_cast<StatusCodes::StatusCodes>(response->StatusCode);
        Message::DeleteMessage(response);
        return (status);
    }
};

/**
 * @brief Build a password of a number of characters.
 *
 * @param buffer Buffer of PASSWORD_BUFFER_SIZE bytes to build it in.
 * @param length Number of characters.
 *
 * @return The password.
 */
static const char *PasswordOfLength(char *buffer, const size_t length)
{
    TEST_ASSERT_LESS_THAN(PASSWORD_BUFFER_SIZE, length);
    memset(buffer, 'a', length);
    buffer[length] = '\0';
    return (buffer);
}

TEST_CASE("A soft-AP password is valid only as a passphrase of 8 to 63 characters", CLASS_OF_TESTS)
{
    char password[PASSWORD_BUFFER_SIZE];

    TEST_ASSERT_FALSE_MESSAGE(TestAccessPointPassword::IsValid(nullptr), "No password was accepted.");
    TEST_ASSERT_FALSE_MESSAGE(TestAccessPointPassword::IsValid(PasswordOfLength(password, 0)), "An empty password was accepted.");
    TEST_ASSERT_FALSE_MESSAGE(TestAccessPointPassword::IsValid(PasswordOfLength(password, 1)), "A password of 1 character was accepted.");
    TEST_ASSERT_FALSE_MESSAGE(TestAccessPointPassword::IsValid(PasswordOfLength(password, 7)), "A password of 7 characters was accepted.");
    TEST_ASSERT_TRUE_MESSAGE(TestAccessPointPassword::IsValid(PasswordOfLength(password, 8)), "A password of 8 characters was refused.");
    TEST_ASSERT_TRUE(TestAccessPointPassword::IsValid(PasswordOfLength(password, 20)));
    TEST_ASSERT_TRUE_MESSAGE(TestAccessPointPassword::IsValid(PasswordOfLength(password, 63)), "A password of 63 characters was refused.");
    TEST_ASSERT_FALSE_MESSAGE(TestAccessPointPassword::IsValid(PasswordOfLength(password, 64)), "A password of 64 characters was accepted.");
    TEST_ASSERT_FALSE(TestAccessPointPassword::IsValid(PasswordOfLength(password, 65)));
}

TEST_CASE("A soft-AP request is checked for its password as well as its name", CLASS_OF_TESTS)
{
    char password[PASSWORD_BUFFER_SIZE];
    StatusCodes::StatusCodes status = StatusCodes::CompletedOk;

    TEST_ASSERT_TRUE_MESSAGE(TestAccessPointPassword::Accepts("MeadowAP", PasswordOfLength(password, 8), status), "A name and an 8 character password were refused.");

    TEST_ASSERT_FALSE(TestAccessPointPassword::Accepts("MeadowAP", PasswordOfLength(password, 7), status));
    TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::InvalidWiFiCredentials, status, "A password that is too short was not reported as invalid credentials.");

    //
    //  Anything else wrong with the request is still reported as it was.
    //
    TEST_ASSERT_FALSE(TestAccessPointPassword::Accepts("", PasswordOfLength(password, 12), status));
    TEST_ASSERT_EQUAL(StatusCodes::CannotStartAccessPoint, status);
}

TEST_CASE("A soft-AP request with a password that cannot be used is refused before the interface is touched", CLASS_OF_TESTS)
{
    char password[PASSWORD_BUFFER_SIZE];
    const size_t lengths[] = {0, 1, 7, 64};

    for (const size_t length: lengths)
    {
        bool interfaceTouched = true;
        const StatusCodes::StatusCodes status = TestAccessPointPassword::Start("MeadowAP", PasswordOfLength(password, length), interfaceTouched);
        char message[64];
        snprintf(message, sizeof(message), "A password of %u characters.", static_cast<unsigned>(length));
        TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::InvalidWiFiCredentials, status, message);
        TEST_ASSERT_FALSE_MESSAGE(interfaceTouched, message);
    }
}
