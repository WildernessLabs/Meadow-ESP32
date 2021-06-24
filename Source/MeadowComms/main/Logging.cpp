/*
 *  Logging.cpp
 *
 *  The Logging class provides the functionality necessary to
 *  setup and run the logging system.
 */
#include "../build/config/sdkconfig.h"

#include "esp32/rom/gpio.h"
#include <limits.h>

#include "SharedEnums.hpp"
#include "Logging.hpp"

#include "AppMain.hpp"
#include "NvsManager.hpp"
#include "MessageDispatcher.hpp"
#include "SpiTransportProvider.hpp"
#include "WiFiRequestHandler.hpp"
#include "SpiTransportProvider.hpp"


//#define BT_DEBUG
//#define SPI_DEBUG

#if defined(PERFORMANCE_LOGGING)

#warning "Performance logging is enabled."

#endif

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

#if defined(DEBUG) || defined(WIFI_DEBUG) || defined(SPI_DEBUG) || defined(THREAD_DEBUG) || defined(SYSTEM_DEBUG) || defined(BT_DEBUG)
    #warning "Debug is enabled."
    esp_log_level_set(APP_MAIN_COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(WIFI_DEBUG) || defined(SYSTEM_DEBUG)
    //
    //  We enable the RequestHandlerBase as it is the base class for the WiFi, System etc. request handlers.
    //
    esp_log_level_set(RequestHandlerBase::COMPONENT_NAME, ESP_LOG_INFO);
    esp_log_level_set(TransportProviderBase::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(WIFI_DEBUG)
    #warning "WIFI debugging is enabled."
    esp_log_level_set(WiFiRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(SPI_DEBUG)
    #warning "SPI debugging is enabled."
    esp_log_level_set(SpiTransportProvider::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(THREAD_DEBUG)
    #warning "ThreadPool debugging is enabled."
    esp_log_level_set(Thread::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(BT_DEBUG)
    #warning "Bluetooth debugging is enabled."
    esp_log_level_set(BluetoothRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(SYSTEM_DEBUG)
    #warning "System debugging is enabled."
    esp_log_level_set(SystemRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
#endif

    const uart_config_t loggingUARTConfig =
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .use_ref_tick = false
    };
    uart_param_config(LOGGING_UART_PORT_NUMBER, &loggingUARTConfig);
    uart_set_pin(LOGGING_UART_PORT_NUMBER, Gpio::LOGGING_TX_PIN, Gpio::LOGGING_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //
    //  We won't use a buffer for sending data.
    //
    uart_driver_install(LOGGING_UART_PORT_NUMBER, RX_BUFFER_SIZE * 2, 0, 0, NULL, 0);
    ESP_LOGI(COMPONENT_NAME, "Logging redirected to UART_NUM_0 on pins %d (Tx), %d (Rx)", Gpio::LOGGING_TX_PIN, Gpio::LOGGING_RX_PIN);
    ESP_LOGI(COMPONENT_NAME, "Logging setup complete.");
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
    switch (type)
    {
        case MessageTypes::Ack:
            return((char *) "Ack");
            break;
        case MessageTypes::Nak:
            return((char *) "Nak");
            break;
        case MessageTypes::Reset:
            return((char *) "Reset");
            break;
        case MessageTypes::Event:
            return((char *) "Event");
            break;
        case MessageTypes::Response:
            return((char *) "Response");
            break;
        case MessageTypes::Transport:
            return((char *) "Transport");
            break;
        case MessageTypes::Header:
            return((char *) "Header");
            break;
        case MessageTypes::Data:
            return((char *) "Data");
            break;
        default:
            return((char *) "Unknown");
            break;
    }
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
    switch (interface)
    {
        case Esp32Interfaces::WiFi:
            return((char *) "WiFi");
            break;
        case Esp32Interfaces::BlueTooth:
            return((char *) "Bluetooth");
            break;
        case Esp32Interfaces::MeshNetwork:
            return((char *) "Mesh Network");
            break;
        case Esp32Interfaces::System:
            return((char *) "System");
            break;
        case Esp32Interfaces::Transport:
            return((char *) "Transport");
            break;
        case Esp32Interfaces::None:
        default:
            return((char *) "Unknown");
            break;
    }
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
    switch (function)
    {
        case WiFiFunction::StartNetwork:
            return((char *) "StartNetwork");
            break;
        case WiFiFunction::Stop:
            return((char *) "Stop");
            break;
        case WiFiFunction::ConnectToAccessPoint:
            return((char *) "ConnectToAccessPoint");
            break;
        case WiFiFunction::DisconnectFromAccessPoint:
            return((char *) "DisconnectFromAccessPoint");
            break;
        case WiFiFunction::GetAccessPoints:
            return((char *) "GetAccessPoints");
            break;
        case WiFiFunction::GetAddrInfo:
            return((char *) "GetAddrInfo");
            break;
        case WiFiFunction::Socket:
            return((char *) "Socket");
            break;
        case WiFiFunction::Connect:
            return((char *) "Connect");
            break;
        case WiFiFunction::Write:
            return((char *) "Write");
            break;
        case WiFiFunction::SetSockOpt:
            return((char *) "SetSockOpt");
            break;
        case WiFiFunction::Read:
            return((char *) "Read");
            break;
        case WiFiFunction::Close:
            return((char *) "Close");
            break;
        case WiFiFunction::SendTo:
            return((char *) "SendTo");
            break;
        case WiFiFunction::RecvFrom:
            return((char *) "RecvFrom");
            break;
        case WiFiFunction::Poll:
            return((char *) "Poll");
            break;
        case WiFiFunction::InterruptPollResponse:
            return((char *) "InterruptPollResponse");
            break;
        case WiFiFunction::Send:
            return((char *) "Send");
            break;
        case WiFiFunction::Bind:
            return((char *) "Bind");
            break;
        case WiFiFunction::Listen:
            return((char *) "Listen");
            break;
        case WiFiFunction::Accept:
            return((char *) "Accept");
            break;
        case WiFiFunction::Ioctl:
            return((char *) "Ioctl");
            break;
        case WiFiFunction::GetSockName:
            return((char *) "GetSockName");
            break;
        case WiFiFunction::GetPeerName:
            return((char *) "GetPeerName");
            break;
        case WiFiFunction::SetTimeOfDayEvent:
            return((char *) "SetTimeOfDayEvent");
            break;
        case WiFiFunction::StartInterfaceEvent:
            return((char *) "StartInterfaceEvent");
            break;
        case WiFiFunction::StopInterfaceEvent:
            return((char *) "StopInterfaceEvent");
            break;
        case WiFiFunction::ConnectEvent:
            return((char *) "ConnectEvent");
            break;
        case WiFiFunction::DisconnectEvent:
            return((char *) "DisconnectEvent");
            break;
        case WiFiFunction::SetAntenna:
            return((char *) "SetAntenna");
            break;
        default:
            return((char *) "Unknown");
            break;
    }
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
    switch (function)
    {
        case SystemFunction::GetConfiguration:
            return((char *) "GetConfiguration");
            break;
        case SystemFunction::SetConfigurationItem:
            return((char *) "SetConfigurationItem");
            break;
        case SystemFunction::DeepSleep:
            return((char *) "DeepSleep");
            break;
        case SystemFunction::GetBatteryChargeLevel:
            return((char *) "GetBatteryChargeLevel");
            break;
        default:
            return((char *) "Unknown");
            break;
    }
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
    switch (function)
    {
        case BluetoothFunction::Start:
            return((char *) "Start");
            break;
        case BluetoothFunction::Stop:
            return((char *) "Stop");
            break;
        default:
            return((char *) "Unknown");
            break;
    }
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
    switch (function)
    {
        case TransportFunction::ResponseReady:
            return((char *) "ResponseReady");
            break;
        case TransportFunction::SendResponse:
            return((char *) "SendResponse");
            break;
        case TransportFunction::KillNuttxThread:
            return((char *) "KillNuttxThread");
            break;
        case TransportFunction::ResetEsp32:
            return((char *) "ResetEsp32");
            break;
        default:
            return((char *) "Unknown");
            break;
    }
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
            return(WiFiFunctionName(function));
            break;
        case Esp32Interfaces::System:
            return(SystemFunctionName(function));
            break;
        case Esp32Interfaces::BlueTooth:
            return(BluetoothFunctionName(function));
            break;
        case Esp32Interfaces::Transport:
            return(TransportFunctionName(function));
            break;
        default:
            return((char *) "Not implemented");
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
    switch (code)
    {
        case StatusCodes::CompletedOk:
            return((char *) "CompletedOk");
            break;
        case StatusCodes::CrcError:
            return((char *) "CrcError");
            break;
        case StatusCodes::Restart:
            return((char *) "Restart");
            break;
        case StatusCodes::Failure:
            return((char *) "Failure");
            break;
        case StatusCodes::InvalidInterface:
            return((char *) "InvalidInterface");
            break;
        case StatusCodes::QueueError:
            return((char *) "QueueError");
            break;
        case StatusCodes::Timeout:
            return((char *) "Timeout");
            break;
        case StatusCodes::InvalidPacket:
            return((char *) "InvalidPacket");
            break;
        case StatusCodes::InvalidHeader:
            return((char *) "InvalidHeader");
            break;
        case StatusCodes::UnexpectedData:
            return((char *) "UnexpectedData");
            break;
        case StatusCodes::MissingEndOfFrameMarker:
            return((char *) "MissingEndOfFrameMarker");
            break;
        case StatusCodes::HeaderBodyFieldMismatch:
            return((char *) "HeaderBodyFieldMismatch");
            break;
        case StatusCodes::WiFiAlreadyStarted:
            return((char *) "WiFiAlreadyStarted");
            break;
        case StatusCodes::InvalidWiFiCredentials:
            return((char *) "InvalidWiFiCredentials");
            break;
        case StatusCodes::WiFiDisconnected:
            return((char *) "WiFiDisconnected");
            break;
        case StatusCodes::CannotStartNetworkInterface:
            return((char *) "CannotStartNetworkInterface");
            break;
        case StatusCodes::CannotConnectToAccessPoint:
            return((char *) "CannotConnectToAccessPoint");
            break;
        case StatusCodes::InvalidAntennaData:
            return((char *) "InvalidAntennaData");
            break;
        case StatusCodes::InvalidAntennaValue:
            return((char *) "InvalidAntennaValue");
            break;
        case StatusCodes::NoMessagesWaiting:
            return((char *) "NoMessagesWaiting");
            break;
        case StatusCodes::CoprocessorNotResponding:
            return((char *) "CoprocessorNotResponding");
            break;
        case StatusCodes::EspWiFiNotStarted:
            return((char *) "EspWiFiNotStarted");
            break;
        case StatusCodes::EspOutOfMemory:
            return((char *) "EspOutOfMemory");
            break;
        case StatusCodes::EspWiFiInvalidSsid:
            return((char *) "EspWiFiInvalidSsid");
            break;
        case StatusCodes::AccessPointNotFound:
            return((char *) "AccessPointNotFound");
            break;
        case StatusCodes::BeaconTimeout:
            return((char *) "BeaconTimeout");
            break;
        case StatusCodes::AuthenticationFailed:
            return((char *) "AuthenticationFailed");
            break;
        case StatusCodes::AssociationFailed:
            return((char *) "AssociationFailed");
            break;
        case StatusCodes::HandshakeTimeout:
            return((char *) "HandshakeTimeout");
            break;
        case StatusCodes::ConnectionFailed:
            return((char *) "ConnectionFailed");
            break;
        case StatusCodes::ApTsfReset:
            return((char *) "ApTsfReset");
            break;
        case StatusCodes::UnmappedErrorCode:
            return((char *) "UnmappedErrorCode");
            break;
        default:
            return((char *) "Unknown");
            break;
    }
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
        ESP_LOGI(componentName, "Message type: 0x%02x (%s)", message->MessageType, MessageTypeName(message->MessageType));
        ESP_LOGI(componentName, "ESP32 Interface: 0x%02x (%s)", message->Interface, InterfaceName(message->Interface));
        ESP_LOGI(componentName, "Message ID: 0x%08x", message->MessageID);
        ESP_LOGI(componentName, "Function to be executed: 0x%08x (%s)", message->Function, FunctionName(message->Interface, message->Function));
        ESP_LOGI(componentName, "Status code: 0x%08x (%s)", message->StatusCode, StatusCodeName(message->StatusCode));
        if (message->PayloadLength > 0)
        {
            ESP_LOGI(componentName, "Payload length: %d", message->PayloadLength);
            if (message->Payload)
            {
                ESP_LOG_BUFFER_HEX(componentName, message->Payload, message->PayloadLength);
            }
            else
            {
                ESP_LOGI(componentName, "Payload has not been set.");
            }
        }
    }
    else
    {
        ESP_LOGI(componentName, "Message pointer is invalid.");
    }
}

/**
 *  @brief Generate a single line summary of the message.
 * 
 *  @param message
 *      Message to be summarised.
 * 
 *  @return
 *      Pointer to a string contianing the message ID, interface and function requested.
 */
char *Logging::MessageSummary(const Message *message)
{
    char buffer[200];

    snprintf(buffer, 200, "ID: 0x%08x Interface: %s Function: %s", message->MessageID, InterfaceName(message->Interface), FunctionName(message->Interface, message->Function));
    return(strdup(buffer));
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
    if (start < end)        // Check if the counter has wrapped around.
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
    ESP_ERROR_CHECK(heap_trace_init_standalone(_heapTraceRecords, NUMBER_OF_HEAP_TRACE_RECORDS));
    ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
    _isStandaloneHeapTracingActive = true;
    _heapEventsRemaining = _heapEventCounterResetValue;
}

/**
 *  @brief Stop standalone heap tracing.
 */
void Logging::StandaloneHeapTracingStop()
{
    ESP_ERROR_CHECK(heap_trace_stop());
    _isStandaloneHeapTracingActive = false;
}

/**
 *  @brief Dump the standalone heap tracing data to the serial port.
 */
void Logging::DumpStandaloneHeapTraceData()
{
    heap_trace_dump();
}

/**
 *  @brief Is heap tracing active?
 */
bool Logging::IsStandaloneHeapTracingActive()
{
    return(_isStandaloneHeapTracingActive);
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