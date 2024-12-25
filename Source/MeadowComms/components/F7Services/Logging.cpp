/*
 *  Logging.cpp
 *
 *  The Logging class provides the functionality necessary to
 *  setup and run the logging system.
 */
#include "sdkconfig.h"

#include "esp32/rom/gpio.h"
#include <string.h>
#include <limits.h>

#include "SharedEnums.hpp"
#include "Logging.hpp"

#include "NvsManager.hpp"
#include "MessageDispatcher.hpp"
#include "SpiTransportProvider.hpp"
#include "WiFiRequestHandler.hpp"
#include "SpiTransportProvider.hpp"
#include "Mapping.hpp"

#if defined(PERFORMANCE_LOGGING)
    #pragma message "Performance logging is enabled."
#endif

#define APP_MAIN_COMPONENT_NAME "AppMain"

/*
 * ---------------------------------------------------------------------------
 *
 *                     Initialise static data.
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component / task.
 */
const char *Logging::COMPONENT_NAME = "Logging";

/**
 *  @brief Is the logging system configured?
 */
bool Logging::_configured = false;

#if defined(PERFORMANCE_LOGGING)

/**
 *  @brief Next performance counter entry to use.
 */
int Logging::_nextPerformanceCounterEntry = 0;

/**
 *  @brief Number of performance counter slots in the circular buffer.
 */
int Logging::_numberOfPerformanceCounterSlots = MAX_PERFORMANCE_COUNTERS;

/**
 *  @brief Table to hold the performance counters;
 */
Logging::PerformanceCounterEntry *Logging::_performanceCounters[MAX_PERFORMANCE_COUNTERS];

#endif

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

/**
 * @brief Has the heap tracing system already setup?
 */
bool Logging::_isStandaloneHeapTracingSetup = false;

/**
 *  @brief Has standalone heap tracing been started?
 */
bool Logging::_isStandaloneHeapTracingActive = false;

/**
 *  @brief Number of events remaining before we dump the standalone heap trace data.
 */
uint32_t Logging::_heapEventsRemaining = 0;

/**
 *  @brief Number of events that can occur before the system dumps the trace data.
 *
 *  This value is used to reset the _heapEventsRemaining counter.
 */
uint32_t Logging::_heapEventCounterResetValue = 10;

/**
 *  @brief Array holding the heap trace records.
 */
heap_trace_record_t Logging::_heapTraceRecords[Logging::NUMBER_OF_HEAP_TRACE_RECORDS];

#endif

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the logging system.
 */
Logging::Logging()
{
    esp_log_level_set(Logging::COMPONENT_NAME, ESP_LOG_INFO);

#if defined(PERFORMANCE_LOGGING)
    for (int index = 0; index < _numberOfPerformanceCounterSlots; index++)
    {
        _performanceCounters[index] = NULL;
    }
#endif
}

/**
 *  @brief Default destructor for the logging system.
 */
Logging::~Logging()
{
}

/*
 * ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Setup the logging system.
 *
 *  The ESP32 should be set up to use custom logging (use 'make menuconfig' to do this) with
 *  the Tx pin set to GPIO22 and the Rx pin set to some other value (21 is used at the moment).
 *  This method then sets up the UART (should be UART0) so that the logging system can output
 *  data using this port.
 */
void Logging::Setup()
{
    esp_log_level_set("*", ESP_LOG_NONE);

    const uart_config_t loggingUARTConfig = {.baud_rate = 115200, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .rx_flow_ctrl_thresh = 0, .source_clk = UART_SCLK_DEFAULT, .flags = 0};
    uart_param_config(LOGGING_UART_PORT_NUMBER, &loggingUARTConfig);
    uart_set_pin(LOGGING_UART_PORT_NUMBER, Gpio::LOGGING_TX_PIN, Gpio::LOGGING_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //
    //  We won't use a buffer for sending data.
    //
    uart_driver_install(LOGGING_UART_PORT_NUMBER, RX_BUFFER_SIZE * 2, 0, 0, NULL, 0);
    TRACE_MESSAGE("Logging redirected to UART_NUM_0 on pins %d (Tx), %d (Rx)", Gpio::LOGGING_TX_PIN, Gpio::LOGGING_RX_PIN);
    TRACE_MESSAGE("Logging setup complete.");
}

/**
 * @brief Configure the logging system.
 *
 * @param components Names of the components to be logged.
 * @param interface Destination for the log messages.
 * @param udpPort UDP port for the log messages when UDP is enabled.
 */
void Logging::Configure(char *components, uint32_t interface, uint32_t udpPort)
{
    if (!_configured)
    {
        bool turnAppLoggingOn = false;

        if (components != NULL)
        {
            char *residual;
            char *duplicate = strdup(components);
            char *component = strtok_r(duplicate, ";", &residual);
            while (component != NULL)
            {
                turnAppLoggingOn |= TurnLoggingOn(component);
                component = strtok_r(residual, ";", &residual);
            }
            free(duplicate);
        }

        if (turnAppLoggingOn)
        {
            esp_log_level_set(APP_MAIN_COMPONENT_NAME, ESP_LOG_INFO);
        }

        _configured = true;
    }
}

/**
 * @brief Turn logging on for the specified component.
 *
 * @note The component name specified must match one of the COMPONENT_NAME entries in the various objects in the system.
 *       Unknown component names will be ignored.
 *
 * @param component Name of the component which should have logging enabled.
 *
 * @return True if the component was found and logging was enabled, false otherwise.
 */
bool Logging::TurnLoggingOn(char *component)
{
    if (component != NULL)
    {
        //
        //  A multitude of if statements are used here because we have some cases where we want to turn logging on for
        //  a group of components and one case where we want to do this for a "non-component" name.
        //
        if (!strcasecmp(component, WiFiRequestHandler::COMPONENT_NAME))
        {
            esp_log_level_set(WiFiRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
            return (true);
        }
        if (!strcasecmp(component, SystemRequestHandler::COMPONENT_NAME))
        {
            esp_log_level_set(SystemRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
            esp_log_level_set(FileSystem::COMPONENT_NAME, ESP_LOG_INFO);
            return (true);
        }
        if (!strcasecmp(component, BluetoothRequestHandler::COMPONENT_NAME))
        {
            esp_log_level_set(BluetoothRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
            return (true);
        }
        if (!strcasecmp(component, SpiTransportProvider::COMPONENT_NAME))
        {
            esp_log_level_set(SpiTransportProvider::COMPONENT_NAME, ESP_LOG_INFO);
            return (true);
        }
        if (!strcasecmp(component, Thread::COMPONENT_NAME))
        {
            esp_log_level_set(Thread::COMPONENT_NAME, ESP_LOG_INFO);
            esp_log_level_set(ThreadPool::COMPONENT_NAME, ESP_LOG_INFO);
            return (true);
        }
        if (!strcasecmp(component, "Messages"))
        {
            esp_log_level_set(RequestHandlerBase::COMPONENT_NAME, ESP_LOG_INFO);
            esp_log_level_set(TransportProviderBase::COMPONENT_NAME, ESP_LOG_INFO);
            esp_log_level_set(MessageDispatcher::COMPONENT_NAME, ESP_LOG_INFO);
            return (true);
        }
    }

    return (false);
}

/**
 *  @brief Process the request to output an informational message to the logging system.
 *
 *  @param methodName
 *      Name of the method that called the Information method.
 *
 *  @param formatString
 *      Format string for the message.
 */
void Logging::Information(char *methodName, char *formatString, ...)
{
}

/**
 *  @brief Process the request to output an error message to the logging system
 *
 *  @param methodName
 *      Name of the method that called the Information method.
 *
 *  @param formatString
 *      Format string for the message.
 */
void Logging::Error(char *methodName, char *formatString, ...)
{
}

/**
 *  @brief Convert the message type enum into a meaningful string.
 *
 *  @param type
 *      Message type code to be converted.
 *
 *  @return
 *      Meaningful string representation of the message type.
 */
char *Logging::MessageTypeName(uint8_t type)
{
    return (Mapping::GetStringValue(Mapping::MessageTypes, type));
}

/**
 *  @brief Convert the interface number into a meaningful string name.
 *
 *  @param interface
 *      Interface number.
 *
 *  @return
 *      Meaningful string representation of the interface number.
 */
char *Logging::InterfaceName(uint32_t interface)
{
    return (Mapping::GetStringValue(Mapping::Interfaces, interface));
}

/**
 *  @brief Convert a WiFi function number into a meaningful string.
 *
 *  @param function
 *      Number of the function to be converted.
 *
 *  @return
 *      Meaningful string representation of the WiFi function.
 */
char *Logging::WiFiFunctionName(uint32_t function)
{
    return (Mapping::GetStringValue(Mapping::WiFiFunctions, function));
}

/**
 *  @brief Convert a System function number into a meaningful string.
 *
 *  @param function
 *      Number of the function to be converted.
 *
 *  @return
 *      Meaningful string representation of the System function.
 */
char *Logging::SystemFunctionName(uint32_t function)
{
    return (Mapping::GetStringValue(Mapping::SystemFunctions, function));
}

/**
 *  @brief Convert a Bluetooth function number into a meaningful string.
 *
 *  @param function
 *      Number of the function to be converted.
 *
 *  @return
 *      Meaningful string representation of the Bluetooth function.
 */
char *Logging::BluetoothFunctionName(uint32_t function)
{
    return (Mapping::GetStringValue(Mapping::BluetoothFunctions, function));
}

/**
 *  @brief Convert a Transport function number into a meaningful string.
 *
 *  @param function
 *      Number of the function to be converted.
 *
 *  @return
 *      Meaningful string representation of the Transport function.
 */
char *Logging::TransportFunctionName(uint32_t function)
{
    return (Mapping::GetStringValue(Mapping::TransportFunctions, function));
}

/**
 *  @brief Convert a function number for a specific interface into a meaningful string.
 *
 *  @param interface
 *      Interface the function is destined for.
 *
 *  @param function
 *      Function number for the interface.
 *
 *  @return
 *      Meaningful string representation of the function.
 */
char *Logging::FunctionName(uint32_t interface, uint32_t function)
{
    switch (interface)
    {
        case Esp32Interfaces::WiFi:
            return (WiFiFunctionName(function));
            break;
        case Esp32Interfaces::System:
            return (SystemFunctionName(function));
            break;
        case Esp32Interfaces::BlueTooth:
            return (BluetoothFunctionName(function));
            break;
        case Esp32Interfaces::Transport:
            return (TransportFunctionName(function));
            break;
        default:
            return (const_cast<char *>("Not implemented"));
            break;
    }
}

/**
 *  @brief Convert the StatusCode enum into a readable string.
 *
 *  @param code
 *      Status code to be converted.
 *
 *  @return
 *      Meaningful string representation of the code.
 */
char *Logging::StatusCodeName(uint32_t code)
{
    return (Mapping::GetStringValue(Mapping::StatusCodes, code));
}

/**
 *  @brief Decode a message structure and output the contents as an information message.
 *
 *  @param componentName
 *      Name of the component where the call originated.
 *
 *  @param message
 *      Pointer to the message to be decoded.
 */
void Logging::DumpMessage(const char *componentName, const Message *message)
{
    if (message)
    {
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Message type: 0x%02x (%s)", message->MessageType, MessageTypeName(message->MessageType));
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "ESP32 Interface: 0x%02x (%s)", message->Interface, InterfaceName(message->Interface));
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Message ID: 0x%08x", (uint) message->MessageID);
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Packet offset (length): %d (%d) bytes", message->PacketOffset, message->PacketLength);
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Function to be executed: 0x%08x (%s)", (uint) message->Function, FunctionName(message->Interface, message->Function));
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Status code: 0x%08x (%s)", (uint) message->StatusCode, StatusCodeName(message->StatusCode));
        if (message->PayloadLength > 0)
        {
            TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Payload length: %lu", message->PayloadLength);
            if (message->Payload)
            {
                TRACE_HEX_BUFFER_SPECIFY_COMPONENT(componentName, message->Payload, message->PayloadLength);
            }
            else
            {
                TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Payload has not been set.");
            }
        }
    }
    else
    {
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Message pointer is invalid.");
    }
}

/**
 *  @brief Generate a single line summary of the message.
 *
 *  @param message
 *      Message to be summarised.
 *
 *  @return
 *      Pointer to a string containing the message ID, interface and function requested.
 */
char *Logging::MessageSummary(const Message *message)
{
    char buffer[200];

    snprintf(buffer, 200, "ID: 0x%08x Interface: %s Function: %s", (uint) message->MessageID, InterfaceName(message->Interface), FunctionName(message->Interface, message->Function));
    return (strdup(buffer));
}

#if defined(PERFORMANCE_LOGGING)

/**
 *  @brief Add an entry to the table of performance counters.
 *
 *  @param method
 *      Name of the method (or event) being recorded.
 *
 *  @param start
 *      Time since the chip started in microseconds of the start of the event.
 *
 *  @param end
 *      Time since the chip started in microseconds of the end of the event.
 */
void Logging::AddPerformanceCounterEntry(const char *method, int64_t start, int64_t end)
{
    uint64_t duration;
    if (start < end) // Check if the counter has wrapped around.
    {
        duration = end - start;
    }
    else
    {
        duration = LONG_LONG_MAX - start + end;
    }
    int pd = duration & 0xffffffff;
    PerformanceCounterEntry *pce = static_cast<PerformanceCounterEntry *>(pvPortMalloc(sizeof(PerformanceCounterEntry)));

    pce->timestamp = esp_log_timestamp();
    pce->method = method;
    pce->duration = pd;
    if (_performanceCounters[_nextPerformanceCounterEntry] != NULL)
    {
        vPortFree(_performanceCounters[_nextPerformanceCounterEntry]);
    }
    _performanceCounters[_nextPerformanceCounterEntry] = pce;
    _nextPerformanceCounterEntry++;
    if (_nextPerformanceCounterEntry >= _numberOfPerformanceCounterSlots)
    {
        _nextPerformanceCounterEntry = 0;
    }
}

/**
 *  @brief Dump the table of performance counters.
 */
void Logging::DumpPerformanceCounters()
{
    printf("Timestamp,Method,Duration\n");
    for (int index = 0; index < _numberOfPerformanceCounterSlots; index++)
    {
        if (_performanceCounters[index] != NULL)
        {
            PerformanceCounterEntry *pce = _performanceCounters[index];
            printf("%u,%s,%u\n", pce->timestamp, pce->method, pce->duration);
        }
    }
}

#endif

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

/**
 *  @brief Start standalone heap tracing.
 */
void Logging::StandaloneHeapTracingStart()
{
    if (!_isStandaloneHeapTracingActive)
    {
        TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, " ");
        TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, "************************************************************");
        TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, "                 Heap Logging Turned On");
        TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, "************************************************************");
        if (!_isStandaloneHeapTracingSetup)
        {
            ESP_ERROR_CHECK(heap_trace_init_standalone(_heapTraceRecords, NUMBER_OF_HEAP_TRACE_RECORDS));
            _isStandaloneHeapTracingSetup = true;
        }
        ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
        _isStandaloneHeapTracingActive = true;
        _heapEventsRemaining = _heapEventCounterResetValue;
    }
}

/**
 *  @brief Stop standalone heap tracing.
 */
void Logging::StandaloneHeapTracingStop()
{
    if (_isStandaloneHeapTracingActive)
    {
        ESP_ERROR_CHECK(heap_trace_stop());
        _isStandaloneHeapTracingActive = false;
    }
}

/**
 *  @brief Dump the standalone heap tracing data to the serial port.
 *
 *  @param delay Delay in milliseconds before the data is generated.  Default value, 2000ms.
 */
void Logging::DumpStandaloneHeapTraceData(uint32_t delay)
{
    vTaskDelay(delay / portTICK_PERIOD_MS);
    TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, " ");
    TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, "************************************************************");
    TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, "                   Heap Logging Dump");

    heap_trace_dump();

    TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, "************************************************************");
    TRACE_MESSAGE_SPECIFY_COMPONENT(APP_MAIN_COMPONENT_NAME, " ");
}

/**
 *  @brief Is heap tracing active?
 */
bool Logging::IsStandaloneHeapTracingActive()
{
    return (_isStandaloneHeapTracingActive);
}

/**
 *  @brief Register a heap event (used to allow a number of events to occur before we dump the trace data.)
 */
void Logging::RegisterHeapEvent()
{
    _heapEventsRemaining--;
    StandaloneHeapEventCheck();
}

/**
 *  @brief Set the number of events we will let happen before we dump the trace data.
 */
void Logging::SetStandaloneHeapEventResetCounter(uint32_t eventCount)
{
    _heapEventCounterResetValue = eventCount;
    _heapEventsRemaining = eventCount;
}

/**
 *  @brief Check the number of heap events that have happened and dump the trace data (and reset) if necessary.
 */
void Logging::StandaloneHeapEventCheck()
{
    if (_heapEventsRemaining == 0)
    {
        StandaloneHeapTracingStop();
        DumpStandaloneHeapTraceData();
        StandaloneHeapTracingStart();
    }
}

#endif