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
     * @brief Work out the authentication mode for a password and a requested mode.
     *
     * @param password Password asked for.
     * @param requestedMode WiFiAuthenticationMode asked for.
     * @param authenticationMode Set to the mode the soft-AP would be started with.
     *
     * @return The status of the resolution.
     */
    static StatusCodes::StatusCodes Resolve(const char *password, const uint8_t requestedMode, wifi_auth_mode_t &authenticationMode)
    {
        return (WiFiRequestHandler::ResolveAccessPointAuthentication(password, requestedMode, authenticationMode));
    }

    /**
     * @brief Build the soft-AP configuration for access point information.
     *
     * @param information Validated access point information.
     * @param config Set to the configuration.
     */
    static void Configure(const Esp32Messaging::AccessPointInformation *information, wifi_config_t &config)
    {
        WiFiRequestHandler::BuildAccessPointConfiguration(information, config);
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
     * @param authenticationMode WiFiAuthenticationMode asked for.
     * @param channel Channel asked for.
     *
     * @return true if the request was accepted.
     */
    static bool Accepts(const char *ssid, const char *password, StatusCodes::StatusCodes &status, const uint8_t authenticationMode = WiFiAuthenticationMode::WpaWpa2Psk, const uint8_t channel = 0)
    {
        MockMessageDispatcher *dispatcher = new MockMessageDispatcher(nullptr);
        TEST_ASSERT_NOT_NULL(dispatcher);
        WiFiRequestHandler *handler = Handler(dispatcher);

        Message *request = CreateAccessPointMessage(WiFiFunction::StartAccessPoint, ssid, password, 0, 0, 0, authenticationMode, channel);
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
     * @param authenticationMode WiFiAuthenticationMode asked for.
     *
     * @return The status of the response.
     */
    static StatusCodes::StatusCodes Start(const char *ssid, const char *password, bool &interfaceTouched, const uint8_t authenticationMode = WiFiAuthenticationMode::WpaWpa2Psk)
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

        Message *request = CreateAccessPointMessage(WiFiFunction::StartAccessPoint, ssid, password, 0, 0, 0, authenticationMode);
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
    const size_t lengths[] = {1, 7, 64};

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

TEST_CASE("The soft-AP authentication mode follows the password and the mode asked for", CLASS_OF_TESTS)
{
    wifi_auth_mode_t mode = WIFI_AUTH_MAX;

    //
    //  No password is an open soft-AP, whatever mode was asked for.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, TestAccessPointPassword::Resolve("", WiFiAuthenticationMode::Open, mode));
    TEST_ASSERT_EQUAL(WIFI_AUTH_OPEN, mode);
    mode = WIFI_AUTH_MAX;
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, TestAccessPointPassword::Resolve("", WiFiAuthenticationMode::Wpa2Psk, mode));
    TEST_ASSERT_EQUAL_MESSAGE(WIFI_AUTH_OPEN, mode, "No password did not give an open soft-AP.");
    mode = WIFI_AUTH_MAX;
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, TestAccessPointPassword::Resolve(nullptr, WiFiAuthenticationMode::Wpa2Psk, mode));
    TEST_ASSERT_EQUAL_MESSAGE(WIFI_AUTH_OPEN, mode, "A password that decoded as nothing did not give an open soft-AP.");

    //
    //  With a password, the mode asked for is the mode used.
    //
    const struct
    {
        uint8_t Requested;
        wifi_auth_mode_t Expected;
    } supported[] = {
        {WiFiAuthenticationMode::WpaPsk, WIFI_AUTH_WPA_PSK}, {WiFiAuthenticationMode::Wpa2Psk, WIFI_AUTH_WPA2_PSK}, {WiFiAuthenticationMode::WpaWpa2Psk, WIFI_AUTH_WPA_WPA2_PSK}, {WiFiAuthenticationMode::Wpa3Psk, WIFI_AUTH_WPA3_PSK}, {WiFiAuthenticationMode::Wpa2Wpa3Psk, WIFI_AUTH_WPA2_WPA3_PSK},
    };

    for (const auto &entry: supported)
    {
        mode = WIFI_AUTH_MAX;
        TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, TestAccessPointPassword::Resolve("MeadowTest1234", entry.Requested, mode));
        TEST_ASSERT_EQUAL_MESSAGE(entry.Expected, mode, "The authentication mode asked for was not the one used.");
        TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::InvalidWiFiCredentials, TestAccessPointPassword::Resolve("short", entry.Requested, mode), "A passphrase shorter than 8 characters was accepted.");
    }

    //
    //  What a soft-AP cannot do is refused, not changed into something else.
    //
    TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::InvalidWiFiCredentials, TestAccessPointPassword::Resolve("MeadowTest1234", WiFiAuthenticationMode::Open, mode), "An open soft-AP with a password was accepted.");
    TEST_ASSERT_EQUAL(StatusCodes::NotImplemented, TestAccessPointPassword::Resolve("MeadowTest1234", WiFiAuthenticationMode::Wep, mode));
    TEST_ASSERT_EQUAL(StatusCodes::NotImplemented, TestAccessPointPassword::Resolve("MeadowTest1234", WiFiAuthenticationMode::Wpa2Enterprise, mode));
    TEST_ASSERT_EQUAL(StatusCodes::InvalidRequest, TestAccessPointPassword::Resolve("MeadowTest1234", 8, mode));
}

TEST_CASE("A soft-AP request for a channel that does not exist is refused", CLASS_OF_TESTS)
{
    StatusCodes::StatusCodes status = StatusCodes::CompletedOk;

    TEST_ASSERT_TRUE_MESSAGE(TestAccessPointPassword::Accepts("MeadowAP", "MeadowTest1234", status, WiFiAuthenticationMode::Wpa2Psk, 0), "Channel 0, left to the ESP32, was refused.");
    TEST_ASSERT_TRUE(TestAccessPointPassword::Accepts("MeadowAP", "MeadowTest1234", status, WiFiAuthenticationMode::Wpa2Psk, 1));
    TEST_ASSERT_TRUE(TestAccessPointPassword::Accepts("MeadowAP", "MeadowTest1234", status, WiFiAuthenticationMode::Wpa2Psk, 14));
    TEST_ASSERT_FALSE_MESSAGE(TestAccessPointPassword::Accepts("MeadowAP", "MeadowTest1234", status, WiFiAuthenticationMode::Wpa2Psk, 15), "Channel 15 was accepted.");
    TEST_ASSERT_EQUAL(StatusCodes::InvalidRequest, status);
    TEST_ASSERT_FALSE(TestAccessPointPassword::Accepts("MeadowAP", "MeadowTest1234", status, WiFiAuthenticationMode::Wpa2Psk, 255));
    TEST_ASSERT_EQUAL(StatusCodes::InvalidRequest, status);

    //
    //  An open request, and one for a mode a soft-AP cannot do, are refused on the same request.
    //
    TEST_ASSERT_TRUE_MESSAGE(TestAccessPointPassword::Accepts("MeadowAP", "", status, WiFiAuthenticationMode::Open), "An open soft-AP was refused.");
    TEST_ASSERT_FALSE(TestAccessPointPassword::Accepts("MeadowAP", "MeadowTest1234", status, WiFiAuthenticationMode::Open));
    TEST_ASSERT_EQUAL(StatusCodes::InvalidWiFiCredentials, status);
    TEST_ASSERT_FALSE(TestAccessPointPassword::Accepts("MeadowAP", "MeadowTest1234", status, WiFiAuthenticationMode::Wep));
    TEST_ASSERT_EQUAL(StatusCodes::NotImplemented, status);
}

TEST_CASE("The soft-AP configuration holds the mode, channel and hidden flag asked for", CLASS_OF_TESTS)
{
    char name[] = "MeadowAP";
    char password[] = "MeadowTest1234";
    Esp32Messaging::AccessPointInformation information = {};
    information.NetworkName = name;
    information.Password = password;
    information.WiFiAuthenticationMode = WiFiAuthenticationMode::Wpa2Psk;
    information.Channel = 6;
    information.Hidden = 1;

    wifi_config_t config = {};
    TestAccessPointPassword::Configure(&information, config);
    TEST_ASSERT_EQUAL_MESSAGE(WIFI_AUTH_WPA2_PSK, config.ap.authmode, "The authentication mode asked for was not applied.");
    TEST_ASSERT_EQUAL_MESSAGE(6, config.ap.channel, "The channel asked for was not applied.");
    TEST_ASSERT_EQUAL_MESSAGE(1, config.ap.ssid_hidden, "The hidden flag asked for was not applied.");
    TEST_ASSERT_EQUAL_UINT8(strlen(name), config.ap.ssid_len);
    TEST_ASSERT_EQUAL_STRING(name, reinterpret_cast<const char *>(config.ap.ssid));
    TEST_ASSERT_EQUAL_STRING(password, reinterpret_cast<const char *>(config.ap.password));

    //
    //  The defaults leave the channel to the ESP32 and the name visible.
    //
    information.WiFiAuthenticationMode = WiFiAuthenticationMode::WpaWpa2Psk;
    information.Channel = 0;
    information.Hidden = 0;
    TestAccessPointPassword::Configure(&information, config);
    TEST_ASSERT_EQUAL(WIFI_AUTH_WPA_WPA2_PSK, config.ap.authmode);
    TEST_ASSERT_EQUAL(0, config.ap.channel);
    TEST_ASSERT_EQUAL(0, config.ap.ssid_hidden);

    //
    //  No password is an open soft-AP, and WPA3 needs protected management frames.
    //
    char noPassword[] = "";
    information.Password = noPassword;
    TestAccessPointPassword::Configure(&information, config);
    TEST_ASSERT_EQUAL_MESSAGE(WIFI_AUTH_OPEN, config.ap.authmode, "No password did not give an open soft-AP.");

    information.Password = password;
    information.WiFiAuthenticationMode = WiFiAuthenticationMode::Wpa3Psk;
    TestAccessPointPassword::Configure(&information, config);
    TEST_ASSERT_EQUAL(WIFI_AUTH_WPA3_PSK, config.ap.authmode);
    TEST_ASSERT_TRUE(config.ap.pmf_cfg.required);
    information.WiFiAuthenticationMode = WiFiAuthenticationMode::Wpa2Wpa3Psk;
    TestAccessPointPassword::Configure(&information, config);
    TEST_ASSERT_EQUAL(WIFI_AUTH_WPA2_WPA3_PSK, config.ap.authmode);
    TEST_ASSERT_TRUE(config.ap.pmf_cfg.capable);
    TEST_ASSERT_FALSE(config.ap.pmf_cfg.required);
}

TEST_CASE("A soft-AP request for an open soft-AP with a password is refused before the interface is touched", CLASS_OF_TESTS)
{
    bool interfaceTouched = true;
    const StatusCodes::StatusCodes status = TestAccessPointPassword::Start("MeadowAP", "MeadowTest1234", interfaceTouched, WiFiAuthenticationMode::Open);
    TEST_ASSERT_EQUAL(StatusCodes::InvalidWiFiCredentials, status);
    TEST_ASSERT_FALSE(interfaceTouched);
}
