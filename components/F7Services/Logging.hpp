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
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>
#include "Gpio.hpp"

#include "SharedEnums.hpp"
#include "Esp32Messaging.hpp"

//
//  Performance counter definitions.
//
#if defined(PERFORMANCE_LOGGING)

    #define PERFORMANCE_LOGGING_ENTER() int64_t performance_logging_start = esp_timer_get_time()

    #define PERFORMANCE_LOGGING_EXIT() int64_t performance_logging_end = esp_timer_get_time()

    #define PERFORMANCE_LOGGING_ADD_ENTRY() Logging::AddPerformanceCounterEntry(__func__, performance_logging_start, performance_logging_end)

    #define PERFORMANCE_LOGGING_DUMP_RESULTS() Logging::DumpPerformanceCounters()

#else

    #define PERFORMANCE_LOGGING_ENTER()
    #define PERFORMANCE_LOGGING_EXIT()
    #define PERFORMANCE_LOGGING_ADD_ENTRY()
    #define PERFORMANCE_LOGGING_DUMP_RESULTS()

#endif

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
     *  @brief Structure to hold performance information for a specific event.
     */
    struct PerformanceCounterEntry
    {
        /**
         *  @brief Time that this event was logged.
         */
        uint32_t timestamp;

        /**
         *  @brief Name of the method that generated the event.
         */
        const char *method;

        /**
         *  @brief Number of microseconds elapsed during the recorded event.
         */
        uint32_t duration;
    };

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
     * @brief Is the logging system configured?
     *
     * This is used to prevent multiple configuration attempts.
     */
    static bool _configured;

    /**
     * @brief Turn logging on for the specified component.
     */
    static bool TurnLoggingOn(const char * const component);

#if defined(PERFORMANCE_LOGGING)
    /**
     *  @brief Maximum number of performance counters to be held in the circular buffer.
     */
    static const int MAX_PERFORMANCE_COUNTERS = 500;

    /**
     *  @brief Next performance counter entry to use.
     */
    static int _nextPerformanceCounterEntry;

    /**
     *  @brief Number of performance counter slots in the circular buffer.
     */
    static int _numberOfPerformanceCounterSlots;

    /**
     *  @brief Table to hold the performance counters;
     */
    static PerformanceCounterEntry *_performanceCounters[MAX_PERFORMANCE_COUNTERS];
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
    static void Configure(const char * const components, uint32_t interface, uint32_t udpPort);

    /**
     *  @brief Translate the ESP-IDF error codes into StatusCodes.
     */
    static StatusCodes::StatusCodes TranslateEspErrorCode(esp_err_t);

    /**
     *  @brief Convert a message type into a meaningful string.
     */
    static char *MessageTypeName(const uint8_t type);

    /**
     *  @brief Convert the interface enum (Esp32Interfaces) from an interface type into a string name.
     */
    static char *InterfaceName(const uint32_t interface);

    /**
     *  @brief Convert the request enum into a WiFi message name.
     */
    static char *WiFiFunctionName(const uint32_t function);

    /**
     *  @brief Convert the request enum into a System message name.
     */
    static char *SystemFunctionName(const uint32_t function);

    /**
     *  @brief Convert the request enum into a Bluetooth message name.
     */
    static char *BluetoothFunctionName(const uint32_t function);

    /**
     *  @brief Convert the request enum into a Transport message name.
     */
    static char *TransportFunctionName(const uint32_t function);

    /**
     *  @brief Convert the specified function ID into a string name.  The function name will vary between the interfaces.
     */
    static char *FunctionName(const uint32_t interface, const uint32_t function);

    /**
     *  @brief Convert a status code into a meaningful name.
     */
    static char *StatusCodeName(const uint32_t code);

    /**
     *  @brief Decode the message structure and output the components.
     */
    static void DumpMessage(const char * const componentName, const Message * const message);

    /**
     *  @brief Add an entry to the table of performance counters.
     */
    static void AddPerformanceCounterEntry(const char *method, int64_t start, int64_t end);

    /**
     *  @brief Dump the table of performance counters.
     */
    static void DumpPerformanceCounters();

    /**
     *  @brief Generate a single line summary of the message.
     */
    static char *MessageSummary(const Message * const message);
};
