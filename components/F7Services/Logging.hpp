/**
 * @file Logging.hpp
 * @author Mark Stevens
 * @brief The Logging class provides the functionality necessary to setup and run the logging system.
 * @date 2019-01-24
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#pragma once

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "Gpio.hpp"

#include "SharedEnums.hpp"
#include "Esp32Messaging.hpp"

//
//  Trace and debug output macros.
//
#if defined(TRACE_MESSAGES)

    #define TRACE_MESSAGE(format, ...) ESP_LOGI(COMPONENT_NAME, format, ##__VA_ARGS__)
    #define DEBUG_MESSAGE(format, ...) ESP_LOGD(COMPONENT_NAME, format, ##__VA_ARGS__)
    #define ERROR_MESSAGE(format, ...) ESP_LOGE(COMPONENT_NAME, format, ##__VA_ARGS__)

    #define TRACE_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...) ESP_LOGI(component_name, format, ##__VA_ARGS__)
    #define DEBUG_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...) ESP_LOGD(component_name, format, ##__VA_ARGS__)
    #define ERROR_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...) ESP_LOGE(component_name, format, ##__VA_ARGS__)

    #define TRACE_HEX_BUFFER(buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(COMPONENT_NAME, buffer, length, ESP_LOG_INFO)
    #define DEBUG_HEX_BUFFER(buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(COMPONENT_NAME, buffer, length, ESP_LOG_DEBUG)
    #define ERROR_HEX_BUFFER(buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(COMPONENT_NAME, buffer, length, ESP_LOG_ERROR)

    #define TRACE_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(component_name, buffer, length, ESP_LOG_INFO)
    #define DEBUG_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(component_name, buffer, length, ESP_LOG_DEBUG)
    #define ERROR_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length) ESP_LOG_BUFFER_HEX_LEVEL(component_name, buffer, length, ESP_LOG_ERROR)

#else

    #define TRACE_MESSAGE(format, ...)
    #define DEBUG_MESSAGE(format, ...)
    #define ERROR_MESSAGE(format, ...)

    #define TRACE_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...)
    #define DEBUG_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...)
    #define ERROR_MESSAGE_SPECIFY_COMPONENT(component_name, format, ...)

    #define TRACE_HEX_BUFFER(buffer, length)
    #define DEBUG_HEX_BUFFER(buffer, length)
    #define ERROR_HEX_BUFFER(buffer, length)

    #define TRACE_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length)
    #define DEBUG_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length)
    #define ERROR_HEX_BUFFER_SPECIFY_COMPONENT(component_name, buffer, length)

#endif

/**
 *  @brief The Logging class provides the functionality necessary to setup and run the logging system.
 */
class Logging
{
private:
    /**
     *  @brief UART Port number for the logging output.
     */
    static const uart_port_t LOGGING_UART_PORT_NUMBER = UART_NUM_0;

    /**
     *  @brief Number of bytes in the Rx buffer.
     */
    static const int RX_BUFFER_SIZE = 1024;

    /**
     *  @brief Number of bytes in the Rx buffer.
     */
    static const int TX_BUFFER_SIZE = 1024;

    /**
     * @brief Test access to the log destination (issue #356).
     */
    friend class TestLogging;

    /**
     * @brief Where the log messages are going now: one of Esp32LogDestination.
     */
    static uint8_t _destination;

    /**
     * @brief UDP port the log messages are sent to when the destination is UDP.
     */
    static uint16_t _udpPort;

    /**
     * @brief Socket the log messages are sent on when the destination is UDP, -1 if there is none.
     */
    static int _udpSocket;

    /**
     * @brief Address the UDP log messages are sent to, in network byte order: broadcast unless a test changes it.
     */
    static uint32_t _udpAddress;

    /**
     * @brief The function that wrote log messages before the UDP one was put in front of it.
     */
    static vprintf_like_t _previousLogOutput;

    /**
     * @brief Guards the buffer a UDP log message is formatted in.
     */
    static SemaphoreHandle_t _udpMutex;

    /**
     * @brief Largest log message sent in one UDP datagram; a longer one is cut.
     */
    static const int UDP_LOG_MESSAGE_SIZE = 256;

    /**
     * @brief Buffer a UDP log message is formatted in.
     */
    static char _udpBuffer[UDP_LOG_MESSAGE_SIZE];

    /**
     * @brief Set the log level of a component, or of every component in a named group.
     */
    static bool SetComponentLogging(const char * const component, const esp_log_level_t level);

    /**
     * @brief Turn logging on for the specified component.
     */
    static bool TurnLoggingOn(const char * const component);

    /**
     * @brief Turn logging off for every component the configuration can turn on.
     */
    static void TurnAllLoggingOff();

    /**
     * @brief Start sending the log messages over UDP as well as writing them out as before.
     */
    static StatusCodes::StatusCodes StartUdpLogging(const uint16_t port);

    /**
     * @brief Stop sending the log messages over UDP and put back the output that was there before.
     */
    static void StopUdpLogging();

    /**
     * @brief The log output function while UDP logging is on.
     */
    static int UdpLogOutput(const char *format, va_list arguments);

#if defined(CONFIG_APPTRACE_SV_ENABLE)
    /**
     * @brief The function that wrote log messages before they were sent to SystemView, nullptr when they are not.
     */
    static vprintf_like_t _outputBeforeSystemView;

    /**
     * @brief Send the log messages to the SystemView trace instead of the UART.
     */
    static void StartSystemViewLogging();

    /**
     * @brief Stop sending the log messages to SystemView and put back the output that was there before.
     */
    static void StopSystemViewLogging();
#endif

public:
    /**
     *  @brief Name of the component / task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Default constructor for the logging system.
     */
    Logging();

    /**
     *  @brief Default destructor for the logging system.
     */
    ~Logging();

    /**
     * @brief Setup this class.
     */
    static void Setup();

    /**
     * @brief Configure the logging system.
     */
    static StatusCodes::StatusCodes Configure(const char * const components, uint32_t interface, uint32_t udpPort);

    /**
     *  @brief Translate the ESP-IDF error codes into StatusCodes.
     */
    static StatusCodes::StatusCodes TranslateEspErrorCode(esp_err_t);

    /**
     *  @brief Convert a message type into a meaningful string.
     */
    static const char *MessageTypeName(const uint8_t type);

    /**
     *  @brief Convert the interface enum (Esp32Interfaces) from an interface type into a string name.
     */
    static const char *InterfaceName(const uint32_t interface);

    /**
     *  @brief Convert the request enum into a WiFi message name.
     */
    static const char *WiFiFunctionName(const uint32_t function);

    /**
     *  @brief Convert the request enum into a System message name.
     */
    static const char *SystemFunctionName(const uint32_t function);

    /**
     *  @brief Convert the request enum into a Bluetooth message name.
     */
    static const char *BluetoothFunctionName(const uint32_t function);

    /**
     *  @brief Convert the request enum into a Transport message name.
     */
    static const char *TransportFunctionName(const uint32_t function);

    /**
     *  @brief Convert the specified function ID into a string name.  The function name will vary between the interfaces.
     */
    static const char *FunctionName(const uint32_t interface, const uint32_t function);

    /**
     *  @brief Convert a status code into a meaningful name.
     */
    static const char *StatusCodeName(const uint32_t code);

    /**
     *  @brief Decode the message structure and output the components.
     */
    static void DumpMessage(const char * const componentName, const Message * const message);
};
