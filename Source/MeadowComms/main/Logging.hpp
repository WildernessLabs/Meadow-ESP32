/*
 *  Logging.hpp
 *
 *  The Logging class provides the functionality necessary to
 *  setup and run the logging system.
 */

#ifndef MAIN_LOGGING_HPP_
#define MAIN_LOGGING_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>
#include "Gpio.hpp"
#include "Esp32Messaging.hpp"

#if defined(PERFORMANCE_LOGGING)

#define PERFORMANCE_LOGGING_ENTER()             int64_t performance_logging_start = esp_timer_get_time()

#define PERFORMANCE_LOGGING_EXIT()              int64_t performance_logging_end = esp_timer_get_time()

#define PERFORMANCE_LOGGING_ADD_ENTRY()         Logging::AddPerformanceCounterEntry(__func__, performance_logging_start, performance_logging_end)

#define PERFORMANCE_LOGGING_DUMP_RESULTS()      Logging::DumpPerformanceCounters()

#else

#define PERFORMANCE_LOGGING_ENTER()
#define PERFORMANCE_LOGGING_EXIT()
#define PERFORMANCE_LOGGING_ADD_ENTRY(method_name)
#define PERFORMANCE_LOGGING_DUMP_RESULTS()

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
     *  @brief Process the request to output an informational message to the logging system.
     */
    static void Information(char *, char *, ...);

    /**
     *  @brief Process the request to output an error message to the logging system
     */
    static void Error(char *, char *, ...);

    /**
     *  @brief Convert a message type into a meaningful string.
     */
    static char *MessageTypeName(uint8_t);

    /**
     *  @brief Convert the interface enum (Esp32Interfaces) from an interface type into a string name.
     */
    static char *InterfaceName(uint32_t);

    /**
     *  @brief Convert the request enum into a WiFi message name.
     */
    static char *WiFiFunctionName(uint32_t);

    /**
     *  @brief Convert the request enum into a System message name.
     */
    static char *SystemFunctionName(uint32_t);

    /**
     *  @brief Convert the request enum into a Bluetooth message name.
     */
    static char *BluetoothFunctionName(uint32_t);

    /**
     *  @brief Convert the request enum into a Transport message name.
     */
    static char *TransportFunctionName(uint32_t);

    /**
     *  @brief Convert the specified function ID into a string name.  The function name will vary between the interfaces.
     */
    static char *FunctionName(uint32_t, uint32_t);

    /**
     *  @brief Convert a status code into a meaningful name.
     */
    static char *StatusCodeName(uint32_t);

    /**
     *  @brief Decode the message structure and output the components.
     */
    static void DumpMessage(const char *, const Message *);

    /**
     *  @brief Add an entry to the table of performance counters.
     */
    static void AddPerformanceCounterEntry(const char *, int64_t, int64_t);

    /**
     *  @brief Dump the table of performance counters.
     */
    static void DumpPerformanceCounters();

    /**
     *  @brief Generate a single line summary of the message.
     */
    static char *MessageSummary(const Message *);
};


#endif /* MAIN_LOGGING_HPP_ */
