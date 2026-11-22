/*
 *  TestLogging.cpp
 *
 *  Tests that Logging::Configure applies the destination and the UDP port it is given, that a request
 *  replaces the one before it, and that one it cannot honour is refused and changes nothing (issue #356).
 *  Also that the level methods' shared message buffer is neither overwritten nor waited on by a message
 *  logged while another is being written, and that two tasks logging at once both get whole messages.
 */

#include "sdkconfig.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "unity.h"
#ifdef CONFIG_MEADOW_SYSVIEW
    //
    //  esp_sysview_vprintf comes from the espressif/esp_sysview managed component, which is
    //  where SystemView lives from ESP-IDF 6.
    //
    #include "esp_sysview_heap_trace_module.h"
#endif

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

#ifndef CONFIG_MEADOW_SYSVIEW
    TEST_ASSERT_EQUAL_MESSAGE(StatusCodes::NotImplemented, Logging::Configure("Thread", Esp32LogDestination::Jtag, 0), "JTAG logging was accepted in a build without SystemView.");
#endif
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
    Logging::Information(WiFiRequestHandler::COMPONENT_NAME, "udp log message %d", 356);

    char buffer[256] = {};
    const int received = recv(receiver, buffer, sizeof(buffer) - 1, 0);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, received, "No log message arrived on the UDP port.");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, "udp log message 356"), "The log message that arrived is not the one that was logged.");

    //
    //  Back to the UART: nothing more arrives, and the output that was there before is back in place.
    //
    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("WiFi", Esp32LogDestination::Uart, 0));
    TEST_ASSERT_FALSE(TestLogging::UdpIsOn());
    Logging::Information(WiFiRequestHandler::COMPONENT_NAME, "udp log message %d", 357);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, recv(receiver, buffer, sizeof(buffer) - 1, 0), "A log message was sent over UDP after UDP logging was turned off.");
    const vprintf_like_t outputAfter = esp_log_set_vprintf(vprintf);
    esp_log_set_vprintf(outputAfter);
    TEST_ASSERT_TRUE_MESSAGE(outputBefore == outputAfter, "The log output was not put back.");

    close(receiver);
    TestLogging::Reset();
}

#ifdef CONFIG_MEADOW_SYSVIEW

/**
 * @brief The log output function in use now.
 *
 * @return The function esp_log is calling to write messages.
 */
static vprintf_like_t CurrentLogOutput()
{
    const vprintf_like_t current = esp_log_set_vprintf(vprintf);
    esp_log_set_vprintf(current);
    return (current);
}

TEST_CASE("JTAG logging sends the log messages to SystemView and puts the output back afterwards", CLASS_OF_TESTS)
{
    TestLogging::Reset();
    const vprintf_like_t before = CurrentLogOutput();

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("Thread", Esp32LogDestination::Jtag, 0));
    TEST_ASSERT_EQUAL_UINT8(Esp32LogDestination::Jtag, TestLogging::Destination());
    TEST_ASSERT_TRUE(LogsInformation(Thread::COMPONENT_NAME));
    TEST_ASSERT_TRUE_MESSAGE(CurrentLogOutput() == esp_sysview_vprintf, "The log is not going to SystemView.");

    TEST_ASSERT_EQUAL(StatusCodes::CompletedOk, Logging::Configure("Thread", Esp32LogDestination::Uart, 0));
    TEST_ASSERT_TRUE_MESSAGE(CurrentLogOutput() == before, "The log output was not put back.");

    TestLogging::Reset();
}

#endif

/**
 * @brief Tag the message buffer tests log under, so the capturing output can tell their messages from others.
 */
static const char * const BUFFER_TEST_COMPONENT = "LoggingBufferTest";

/**
 * @brief State shared between the message buffer tests and the log output function they install.
 */
struct LogCapture
{
    /**
     * @brief The output that was in place before the test's own.
     */
    vprintf_like_t Previous;

    /**
     * @brief Whether the output should log a second message from inside the first.
     */
    volatile bool LogFromInside;

    /**
     * @brief Whether the second message is being logged now, so everything written belongs to it.
     */
    volatile bool Inside;

    /**
     * @brief The first message the output wrote, as it was when the output came to write it.
     */
    char Outer[128];

    /**
     * @brief The message logged from inside the first.
     */
    char Inner[128];

    /**
     * @brief Number of the test's messages the output has seen.
     */
    volatile uint32_t Seen;

    /**
     * @brief Number of the test's messages that did not arrive whole.
     */
    volatile uint32_t Damaged;
};

/**
 * @brief The capture the installed log output writes to.
 */
static LogCapture _capture;

/**
 * @brief Guards the capture's counts when two tasks log at once.
 */
static portMUX_TYPE _captureLock = portMUX_INITIALIZER_UNLOCKED;

/**
 * @brief Log output for the re-entrancy test: the first message logs a second before it is written.
 *
 * The first message is formatted only after the second has been logged, so if the second had been formatted
 * in the same buffer the first would come out as the second.
 */
static int ReentrantOutput(const char *format, va_list arguments)
{
    va_list copy;
    va_copy(copy, arguments);
    char line[128];
    vsnprintf(line, sizeof(line), format, copy);
    va_end(copy);
    if (_capture.Inside)
    {
        //
        //  The inner message comes in pieces, and only the first carries the tag: keep them all.
        //
        strlcat(_capture.Inner, line, sizeof(_capture.Inner));
        return (0);
    }
    if ((strstr(line, BUFFER_TEST_COMPONENT) == nullptr) || !_capture.LogFromInside)
    {
        return (_capture.Previous(format, arguments));
    }
    _capture.LogFromInside = false;
    _capture.Inside = true;
    Logging::Information(BUFFER_TEST_COMPONENT, "inner message %d", 2);
    _capture.Inside = false;
    vsnprintf(_capture.Outer, sizeof(_capture.Outer), format, arguments);
    return (0);
}

TEST_CASE("A message logged while another is being written does not wait for it or overwrite it", CLASS_OF_TESTS)
{
    memset(&_capture, 0, sizeof(_capture));
    esp_log_level_set(BUFFER_TEST_COMPONENT, ESP_LOG_INFO);
    _capture.Previous = esp_log_set_vprintf(ReentrantOutput);
    _capture.LogFromInside = true;

    const int64_t start = esp_timer_get_time();
    Logging::Information(BUFFER_TEST_COMPONENT, "outer message %d", 1);
    const int64_t elapsedMs = (esp_timer_get_time() - start) / 1000;

    esp_log_set_vprintf(_capture.Previous);
    esp_log_level_set(BUFFER_TEST_COMPONENT, ESP_LOG_NONE);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(_capture.Outer, "outer message 1"), "The first message was overwritten by the one logged from inside it.");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(_capture.Inner, "inner message 2"), "The message logged from inside the first was lost.");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(_capture.Inner, BUFFER_TEST_COMPONENT), "The message logged from inside the first has no prefix.");
    TEST_ASSERT_LESS_THAN_MESSAGE(50, elapsedMs, "The message logged from inside the first waited for the buffer.");
}

/**
 * @brief Number of messages each task logs in the concurrency test.
 */
static const uint32_t MESSAGES_PER_TASK = 500;

/**
 * @brief Log output for the concurrency test: counts the test's messages and those that did not arrive whole.
 *
 * Each message is a letter, a count and the same letter again, so one formatted over by the other task's
 * message has letters that do not match.
 */
static int CountingOutput(const char *format, va_list arguments)
{
    va_list copy;
    va_copy(copy, arguments);
    char line[128];
    vsnprintf(line, sizeof(line), format, copy);
    va_end(copy);
    const char *body = strstr(line, BUFFER_TEST_COMPONENT);
    if (body == nullptr)
    {
        return (_capture.Previous(format, arguments));
    }
    body += strlen(BUFFER_TEST_COMPONENT) + 2;
    char first = 0;
    char last = 0;
    unsigned count = 0;
    const bool whole = (sscanf(body, "%c %u %c", &first, &count, &last) == 3) && (first == last);
    taskENTER_CRITICAL(&_captureLock);
    _capture.Seen = _capture.Seen + 1;
    if (!whole)
    {
        _capture.Damaged = _capture.Damaged + 1;
    }
    taskEXIT_CRITICAL(&_captureLock);
    return (0);
}

/**
 * @brief What each logging task in the concurrency test is given.
 */
struct LoggingTaskArguments
{
    /**
     * @brief Letter the task puts at each end of its messages.
     */
    char Letter;

    /**
     * @brief Given by the test to let the tasks start together.
     */
    SemaphoreHandle_t Start;

    /**
     * @brief Given by each task when it has logged its messages.
     */
    SemaphoreHandle_t Finished;
};

/**
 * @brief Log the test's messages as fast as possible.
 */
static void LoggingTask(void *parameters)
{
    const LoggingTaskArguments *arguments = static_cast<LoggingTaskArguments *>(parameters);
    xSemaphoreTake(arguments->Start, portMAX_DELAY);
    for (uint32_t index = 0; index < MESSAGES_PER_TASK; index++)
    {
        Logging::Information(BUFFER_TEST_COMPONENT, "%c %" PRIu32 " %c", arguments->Letter, index, arguments->Letter);
    }
    xSemaphoreGive(arguments->Finished);
    vTaskDelete(nullptr);
}

TEST_CASE("Messages logged by two tasks at once arrive whole", CLASS_OF_TESTS)
{
    memset(&_capture, 0, sizeof(_capture));
    esp_log_level_set(BUFFER_TEST_COMPONENT, ESP_LOG_INFO);
    _capture.Previous = esp_log_set_vprintf(CountingOutput);

    const SemaphoreHandle_t start = xSemaphoreCreateCounting(2, 0);
    const SemaphoreHandle_t finished = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(finished);
    LoggingTaskArguments arguments[2] = {{'A', start, finished}, {'B', start, finished}};
    //
    //  The test runs above the tasks while it starts them, so that neither begins before the other exists.
    //
    const UBaseType_t priority = uxTaskPriorityGet(nullptr);
    vTaskPrioritySet(nullptr, tskIDLE_PRIORITY + 10);
    for (int index = 0; index < 2; index++)
    {
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreatePinnedToCore(LoggingTask, "LoggingTest", 4096, &arguments[index], tskIDLE_PRIORITY + 5, nullptr, index));
    }
    xSemaphoreGive(start);
    xSemaphoreGive(start);
    vTaskPrioritySet(nullptr, priority);
    const bool bothFinished = (xSemaphoreTake(finished, pdMS_TO_TICKS(30000)) == pdTRUE) && (xSemaphoreTake(finished, pdMS_TO_TICKS(30000)) == pdTRUE);

    esp_log_set_vprintf(_capture.Previous);
    esp_log_level_set(BUFFER_TEST_COMPONENT, ESP_LOG_NONE);
    vSemaphoreDelete(start);
    vSemaphoreDelete(finished);

    TEST_ASSERT_TRUE_MESSAGE(bothFinished, "The logging tasks did not finish.");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2 * MESSAGES_PER_TASK, _capture.Seen, "Not every message arrived.");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, _capture.Damaged, "A message was overwritten by the other task's.");
}
