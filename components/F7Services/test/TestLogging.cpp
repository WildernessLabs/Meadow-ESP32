/*
 *  TestLogging.cpp
 *
 *  Tests that Logging::Configure applies the destination and the UDP port it is given, that a request
 *  replaces the one before it, and that one it cannot honour is refused and changes nothing (issue #356).
 */

#include "sdkconfig.h"

#include <string.h>
#include <sys/socket.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "unity.h"

#include "Logging.hpp"
#include "SharedEnums.hpp"
#include "WiFiRequestHandler.hpp"
#include "BluetoothRequestHandler.hpp"
#include "Thread.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Logging][Emulated]"

/**
 * @brief UDP port the tests send log messages to, on the loopback address.
 */
#define TEST_UDP_PORT 40356

/**
 * @brief Gives the tests access to the log destination.
 */
class TestLogging
{
public:
    /**
     * @brief Send the UDP log messages to the loopback address instead of the broadcast address.
     */
    static void SendToLoopback()
    {
        Logging::_udpAddress = htonl(INADDR_LOOPBACK);
    }

    /**
     * @brief Put back the broadcast address.
     */
    static void SendToBroadcast()
    {
        Logging::_udpAddress = 0xffffffff;
    }

    /**
     * @brief The destination the log messages are going to now.
     */
    static uint8_t Destination()
    {
        return (Logging::_destination);
    }

    /**
     * @brief Whether log messages are being sent over UDP.
     */
    static bool UdpIsOn()
    {
        return (Logging::_udpSocket >= 0);
    }

    /**
     * @brief Put logging back as it was found: nothing configured.
     */
    static void Reset()
    {
        Logging::Configure(nullptr, Esp32LogDestination::None, 0);
        SendToBroadcast();
    }
};

/**
 * @brief Whether a component logs at INFO.
 */
static bool LogsInformation(const char *component)
{
    return (esp_log_level_get(component) == ESP_LOG_INFO);
}

TEST_CASE("A logging configuration replaces the one before it", CLASS_OF_TESTS)
{
    TestLogging::Reset();

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("WiFi;Bluetooth", Esp32LogDestination::Uart, 0));
    TEST_ASSERT_TRUE(LogsInformation(WiFiRequestHandler::COMPONENT_NAME));
    TEST_ASSERT_TRUE(LogsInformation(BluetoothRequestHandler::COMPONENT_NAME));
    TEST_ASSERT_FALSE(LogsInformation(Thread::COMPONENT_NAME));

    //
    //  The second request used to be ignored, as only the first was ever applied.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("Thread", Esp32LogDestination::Uart, 0));
    TEST_ASSERT_TRUE_MESSAGE(LogsInformation(Thread::COMPONENT_NAME), "A second configuration was not applied.");
    TEST_ASSERT_FALSE_MESSAGE(LogsInformation(WiFiRequestHandler::COMPONENT_NAME), "A second configuration did not turn off what the first turned on.");
    TEST_ASSERT_FALSE(LogsInformation(BluetoothRequestHandler::COMPONENT_NAME));
    TEST_ASSERT_EQUAL_UINT8(Esp32LogDestination::Uart, TestLogging::Destination());

    TestLogging::Reset();
}

TEST_CASE("No log destination turns all logging off", CLASS_OF_TESTS)
{
    TestLogging::Reset();
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("WiFi", Esp32LogDestination::Uart, 0));
    TEST_ASSERT_TRUE(LogsInformation(WiFiRequestHandler::COMPONENT_NAME));

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("WiFi;Bluetooth", Esp32LogDestination::None, 0));
    TEST_ASSERT_FALSE_MESSAGE(LogsInformation(WiFiRequestHandler::COMPONENT_NAME), "Components were logged although the destination was None.");
    TEST_ASSERT_FALSE(LogsInformation(BluetoothRequestHandler::COMPONENT_NAME));
    TEST_ASSERT_EQUAL_UINT8(Esp32LogDestination::None, TestLogging::Destination());

    TestLogging::Reset();
}

TEST_CASE("A logging configuration that cannot be honoured is refused and changes nothing", CLASS_OF_TESTS)
{
    TestLogging::Reset();
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("WiFi", Esp32LogDestination::Uart, 0));

    TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::NotImplemented, Logging::Configure("Thread", Esp32LogDestination::Jtag, 0), "JTAG logging was accepted.");
    TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::InvalidRequest, Logging::Configure("Thread", 9, 0), "A destination that does not exist was accepted.");
    TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::InvalidRequest, Logging::Configure("Thread", Esp32LogDestination::UDP, 0), "UDP logging with no port was accepted.");
    TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::InvalidRequest, Logging::Configure("Thread", Esp32LogDestination::UDP, 70000), "UDP logging to a port above 65535 was accepted.");

    TEST_ASSERT_TRUE_MESSAGE(LogsInformation(WiFiRequestHandler::COMPONENT_NAME), "A refused request changed what was being logged.");
    TEST_ASSERT_FALSE(LogsInformation(Thread::COMPONENT_NAME));
    TEST_ASSERT_EQUAL_UINT8(Esp32LogDestination::Uart, TestLogging::Destination());
    TEST_ASSERT_FALSE(TestLogging::UdpIsOn());

    TestLogging::Reset();
}

/**
 * @brief Open a socket on the loopback address to receive the log messages.
 *
 * @return The socket.
 */
static int OpenLogReceiver()
{
    const int receiver = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, receiver);
    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_len = sizeof(address);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(TEST_UDP_PORT);
    TEST_ASSERT_EQUAL_INT(0, bind(receiver, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)));
    struct timeval timeout = {};
    timeout.tv_usec = 500000;
    TEST_ASSERT_EQUAL_INT(0, setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));
    return (receiver);
}

TEST_CASE("Log messages are sent over UDP to the port asked for, and stop when the destination changes", CLASS_OF_TESTS)
{
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_init());
    TestLogging::Reset();
    TestLogging::SendToLoopback();
    const vprintf_like_t outputBefore = esp_log_set_vprintf(vprintf);
    esp_log_set_vprintf(outputBefore);
    const int receiver = OpenLogReceiver();

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("WiFi", Esp32LogDestination::UDP, TEST_UDP_PORT));
    TEST_ASSERT_TRUE(TestLogging::UdpIsOn());
    ESP_LOGI(WiFiRequestHandler::COMPONENT_NAME, "udp log message %d", 356);

    char buffer[256] = {};
    const int received = recv(receiver, buffer, sizeof(buffer) - 1, 0);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, received, "No log message arrived on the UDP port.");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, "udp log message 356"), "The log message that arrived is not the one that was logged.");

    //
    //  Back to the UART: nothing more arrives, and the output that was there before is back in place.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("WiFi", Esp32LogDestination::Uart, 0));
    TEST_ASSERT_FALSE(TestLogging::UdpIsOn());
    ESP_LOGI(WiFiRequestHandler::COMPONENT_NAME, "udp log message %d", 357);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, recv(receiver, buffer, sizeof(buffer) - 1, 0), "A log message was sent over UDP after UDP logging was turned off.");
    const vprintf_like_t outputAfter = esp_log_set_vprintf(vprintf);
    esp_log_set_vprintf(outputAfter);
    TEST_ASSERT_TRUE_MESSAGE(outputBefore == outputAfter, "The log output was not put back.");

    close(receiver);
    TestLogging::Reset();
}
