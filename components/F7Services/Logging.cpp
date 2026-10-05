/**
 * @file Logging.cpp
 * @author Mark Stevens
 * @brief The Logging class provides the functionality necessary to setup and run the logging system.
 * @date 2019-01-24
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#include "sdkconfig.h"

#include "esp32/rom/gpio.h"
#include "lwip/sockets.h"
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
#include "Diagnostics.hpp"
#include "Heap.hpp"
#include "Utils.hpp"
#include "MutexLock.hpp"
#include "AppMain.hpp"

#include "SystemRequestHandler.hpp"
#include "BluetoothRequestHandler.hpp"
#include "WiFiRequestHandler.hpp"
#include "FileSystem.hpp"
#include "Thread.hpp"
#include "ThreadPool.hpp"
#include "CoreDump.hpp"

#if defined(PERFORMANCE_LOGGING)
    #pragma message "Performance logging is enabled."
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

/**
 *  @brief Where the log messages are going now: one of Esp32LogDestination.
 */
uint8_t Logging::_destination = Esp32LogDestination::Uart;

/**
 *  @brief UDP port the log messages are sent to when the destination is UDP.
 */
uint16_t Logging::_udpPort = 0;

/**
 *  @brief Socket the log messages are sent on when the destination is UDP, -1 if there is none.
 */
int Logging::_udpSocket = -1;

/**
 *  @brief Address the UDP log messages are sent to, in network byte order: broadcast unless a test changes it.
 */
uint32_t Logging::_udpAddress = 0xffffffff;

/**
 *  @brief The function that wrote log messages before the UDP one was put in front of it.
 */
vprintf_like_t Logging::_previousLogOutput = nullptr;

/**
 *  @brief Guards the buffer a UDP log message is formatted in.
 */
SemaphoreHandle_t Logging::_udpMutex = nullptr;

/**
 *  @brief Buffer a UDP log message is formatted in.
 */
char Logging::_udpBuffer[Logging::UDP_LOG_MESSAGE_SIZE];

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
        _performanceCounters[index] = nullptr;
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

    const uart_config_t loggingUARTConfig = {.baud_rate = 115200, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .rx_flow_ctrl_thresh = 0, .source_clk = UART_SCLK_DEFAULT, .flags = {}};
    uart_param_config(LOGGING_UART_PORT_NUMBER, &loggingUARTConfig);
    uart_set_pin(LOGGING_UART_PORT_NUMBER, Gpio::LOGGING_TX_PIN, Gpio::LOGGING_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //
    //  We won't use a buffer for sending data.
    //
    uart_driver_install(LOGGING_UART_PORT_NUMBER, RX_BUFFER_SIZE * 2, 0, 0, nullptr, 0);

#if defined(CONFIG_ENABLE_DIAGNOSTIC_THREAD)
    esp_log_level_set(Diagnostics::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(CONFIG_ENABLE_HEAP_INTEGRITY_CHECKS)
    #pragma message "Heap integrity checks enabled, setting heap logging level to INFO."
    esp_log_level_set(Heap::COMPONENT_NAME, ESP_LOG_INFO);
#endif

    TRACE_MESSAGE("Logging redirected to UART_NUM_0 on pins %" PRIu32 " (Tx), %" PRIu32 " (Rx)", Gpio::LOGGING_TX_PIN, Gpio::LOGGING_RX_PIN);
    TRACE_MESSAGE("Logging setup complete.");
}

/**
 * @brief Configure the logging system.
 *
 * A request replaces the one before it: logging is turned off for every component first, then turned on for
 * the ones named, so a second request can both add and remove.  The request is checked before anything is
 * changed, so one that cannot be honoured leaves the configuration as it was.
 *
 * @param components Names of the components to be logged.
 * @param interface Destination for the log messages: Esp32LogDestination None, Uart or UDP.
 * @param udpPort UDP port for the log messages when UDP is enabled.
 *
 * @return CompletedOk if the configuration was applied, InvalidRequest if the destination is not one that
 *         exists or UDP is asked for with no port, NotImplemented if it is JTAG, and Failure if the UDP
 *         socket cannot be opened (issue #356).
 */
StatusCodes::StatusCodes Logging::Configure(const char * const components, uint32_t interface, uint32_t udpPort)
{
    switch (interface)
    {
        case Esp32LogDestination::None:
        case Esp32LogDestination::Uart:
            break;
        case Esp32LogDestination::UDP:
            if ((udpPort == 0) || (udpPort > UINT16_MAX))
            {
                return (StatusCodes::InvalidRequest);
            }
            break;
        case Esp32LogDestination::Jtag:
            return (StatusCodes::NotImplemented);
        default:
            return (StatusCodes::InvalidRequest);
    }

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    StopUdpLogging();
    TurnAllLoggingOff();
    if (interface == Esp32LogDestination::UDP)
    {
        result = StartUdpLogging(static_cast<uint16_t>(udpPort));
    }

    if ((result == StatusCodes::CompletedOk) && (interface != Esp32LogDestination::None))
    {
        bool turnAppLoggingOn = false;

        if (components != nullptr)
        {
            char *duplicate = strdup(components);
            if (duplicate != NULL)
            {
                char *residual;
                char *component = strtok_r(duplicate, ";", &residual);
                while (component != NULL)
                {
                    turnAppLoggingOn |= TurnLoggingOn(component);
                    component = strtok_r(residual, ";", &residual);
                }
                free(duplicate);
            }
        }

        if (turnAppLoggingOn)
        {
            esp_log_level_set(APP_MAIN_COMPONENT_NAME, ESP_LOG_INFO);
        }
    }

    _destination = (result == StatusCodes::CompletedOk) ? static_cast<uint8_t>(interface) : static_cast<uint8_t>(Esp32LogDestination::None);
    return (result);
}

/**
 * @brief Send the log messages over UDP as well as writing them out as before.
 *
 * The messages go to the broadcast address on the port, so anything on the network listening there sees them.
 * The output that was in use is kept and still called, so the UART output goes on.
 *
 * @param port UDP port to send to.
 *
 * @return CompletedOk, or Failure if the socket or the mutex cannot be made.
 */
StatusCodes::StatusCodes Logging::StartUdpLogging(const uint16_t port)
{
    if (_udpMutex == nullptr)
    {
        _udpMutex = xSemaphoreCreateMutex();
        if (_udpMutex == nullptr)
        {
            return (StatusCodes::Failure);
        }
    }

    _udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (_udpSocket < 0)
    {
        _udpSocket = -1;
        return (StatusCodes::Failure);
    }
    const int enable = 1;
    setsockopt(_udpSocket, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));

    _udpPort = port;
    _previousLogOutput = esp_log_set_vprintf(UdpLogOutput);
    return (StatusCodes::CompletedOk);
}

/**
 * @brief Stop sending the log messages over UDP and put back the output that was there before.
 */
void Logging::StopUdpLogging()
{
    if (_udpSocket >= 0)
    {
        esp_log_set_vprintf(_previousLogOutput);
        _previousLogOutput = nullptr;
        //
        //  Taken under the mutex so that a message that is being sent is not sent on a closed socket.
        //
        xSemaphoreTake(_udpMutex, portMAX_DELAY);
        const int socketToClose = _udpSocket;
        _udpSocket = -1;
        xSemaphoreGive(_udpMutex);
        close(socketToClose);
    }
}

/**
 * @brief The log output function while UDP logging is on.
 *
 * It writes the message out as before and also sends it as a datagram.  It never waits: if another task is
 * sending a message at this moment, or the datagram cannot be sent at once, the message goes to the UART only.
 * Nothing in here logs, so it cannot call itself.
 *
 * @param format Format of the message.
 * @param arguments Values for the format.
 *
 * @return What the output that was in use before returned.
 */
int Logging::UdpLogOutput(const char *format, va_list arguments)
{
    if ((_udpSocket >= 0) && (xSemaphoreTake(_udpMutex, 0) == pdTRUE))
    {
        if (_udpSocket >= 0)
        {
            va_list copy;
            va_copy(copy, arguments);
            const int length = vsnprintf(_udpBuffer, sizeof(_udpBuffer), format, copy);
            va_end(copy);
            if (length > 0)
            {
                struct sockaddr_in destination = {};
                destination.sin_family = AF_INET;
                destination.sin_len = sizeof(destination);
                destination.sin_port = htons(_udpPort);
                destination.sin_addr.s_addr = _udpAddress;
                const size_t size = (static_cast<size_t>(length) < sizeof(_udpBuffer)) ? static_cast<size_t>(length) : sizeof(_udpBuffer) - 1;
                sendto(_udpSocket, _udpBuffer, size, MSG_DONTWAIT, reinterpret_cast<struct sockaddr *>(&destination), sizeof(destination));
            }
        }
        xSemaphoreGive(_udpMutex);
    }

    return ((_previousLogOutput != nullptr) ? _previousLogOutput(format, arguments) : vprintf(format, arguments));
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
bool Logging::SetComponentLogging(const char * const component, const esp_log_level_t level)
{
    if (component != NULL)
    {
        //
        //  A multitude of if statements are used here because we have some cases where we want to turn logging on for
        //  a group of components and one case where we want to do this for a "non-component" name.
        //
        if (!strcasecmp(component, WiFiRequestHandler::COMPONENT_NAME))
        {
            esp_log_level_set(WiFiRequestHandler::COMPONENT_NAME, level);
            return (true);
        }
        if (!strcasecmp(component, SystemRequestHandler::COMPONENT_NAME))
        {
            esp_log_level_set(SystemRequestHandler::COMPONENT_NAME, level);
            esp_log_level_set(FileSystem::COMPONENT_NAME, level);
            esp_log_level_set(CoreDump::COMPONENT_NAME, level);
            return (true);
        }
        if (!strcasecmp(component, CoreDump::COMPONENT_NAME))
        {
            esp_log_level_set(CoreDump::COMPONENT_NAME, level);
            return (true);
        }
        if (!strcasecmp(component, BluetoothRequestHandler::COMPONENT_NAME))
        {
            esp_log_level_set(BluetoothRequestHandler::COMPONENT_NAME, level);
            return (true);
        }
        if (!strcasecmp(component, SpiTransportProvider::COMPONENT_NAME))
        {
            esp_log_level_set(SpiTransportProvider::COMPONENT_NAME, level);
            return (true);
        }
        if (!strcasecmp(component, Thread::COMPONENT_NAME))
        {
            esp_log_level_set(Thread::COMPONENT_NAME, level);
            esp_log_level_set(ThreadPool::COMPONENT_NAME, level);
            return (true);
        }
        if (!strcasecmp(component, "Messages"))
        {
            esp_log_level_set(RequestHandlerBase::COMPONENT_NAME, level);
            esp_log_level_set(TransportProviderBase::COMPONENT_NAME, level);
            esp_log_level_set(MessageDispatcher::COMPONENT_NAME, level);
            return (true);
        }
    }

    return (false);
}

/**
 * @brief Turn logging on for the specified component, or for every component in the named group.
 *
 * @param component Name of the component, or the group.
 *
 * @return True if the name was known.
 */
bool Logging::TurnLoggingOn(const char * const component)
{
    return (SetComponentLogging(component, ESP_LOG_INFO));
}

/**
 * @brief Turn logging off for every component, and group of components, the configuration can turn on.
 */
void Logging::TurnAllLoggingOff()
{
    const char * const groups[] = {WiFiRequestHandler::COMPONENT_NAME, SystemRequestHandler::COMPONENT_NAME, CoreDump::COMPONENT_NAME, BluetoothRequestHandler::COMPONENT_NAME, SpiTransportProvider::COMPONENT_NAME, Thread::COMPONENT_NAME, "Messages"};
    for (const char * const group: groups)
    {
        SetComponentLogging(group, ESP_LOG_NONE);
    }
    esp_log_level_set(APP_MAIN_COMPONENT_NAME, ESP_LOG_NONE);
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
const char *Logging::MessageTypeName(const uint8_t type)
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
const char *Logging::InterfaceName(const uint32_t interface)
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
const char *Logging::WiFiFunctionName(const uint32_t function)
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
const char *Logging::SystemFunctionName(const uint32_t function)
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
const char *Logging::BluetoothFunctionName(const uint32_t function)
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
const char *Logging::TransportFunctionName(const uint32_t function)
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
const char *Logging::FunctionName(const uint32_t interface, const uint32_t function)
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
            return ("Not implemented");
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
const char *Logging::StatusCodeName(const uint32_t code)
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
void Logging::DumpMessage(const char * const componentName, const Message * const message)
{
    if (message)
    {
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Message type: 0x%02" PRIx8 " (%s)", message->MessageType, MessageTypeName(message->MessageType));
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "ESP32 Interface: 0x%02" PRIx8 " (%s)", message->Interface, InterfaceName(message->Interface));
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Message ID: 0x%08" PRIx32, message->MessageID);
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Packet offset (length): %" PRIu16 " (%" PRIu16 ") bytes", message->PacketOffset, message->PacketLength);
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Function to be executed: 0x%08" PRIx32 " (%s)", message->Function, FunctionName(message->Interface, message->Function));
        TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Status code: 0x%08" PRIx32 " (%s)", message->StatusCode, StatusCodeName(message->StatusCode));
        if (message->PayloadLength > 0)
        {
            TRACE_MESSAGE_SPECIFY_COMPONENT(componentName, "Payload length: %" PRIu32, message->PayloadLength);
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
char *Logging::MessageSummary(const Message * const message)
{
    char buffer[200];

    snprintf(buffer, 200, "ID: 0x%08" PRIx32 " Interface: %s Function: %s", message->MessageID, InterfaceName(message->Interface), FunctionName(message->Interface, message->Function));
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
 *
 *  @param ownsMethod
 *      True if method is heap-allocated and this call is handing over ownership of
 *      it, so it is freed when the entry is later released.  False (the default,
 *      matching every existing caller) for a string literal such as __func__.
 */
void Logging::AddPerformanceCounterEntry(const char *method, int64_t start, int64_t end, bool ownsMethod)
{
    uint64_t duration;
    if (start <= end) // Check if the counter has wrapped around.
    {
        duration = end - start;
    }
    else
    {
        duration = LONG_LONG_MAX - start + end;
    }
    int pd = duration & 0xffffffff;
    PerformanceCounterEntry *pce = static_cast<PerformanceCounterEntry *>(pvPortMalloc(sizeof(PerformanceCounterEntry)));

    if (pce != nullptr)
    {
        pce->timestamp = esp_log_timestamp();
        pce->method = method;
        pce->ownsMethod = ownsMethod;
        pce->duration = pd;
        //
        //  The slot is claimed and the entry it held taken out under the lock, so no two
        //  tasks get the same slot; the old entry is released after the lock is given back.
        //
        PerformanceCounterEntry *previous = nullptr;
        {
            MutexLock lock(PerformanceCountersMutex());
            previous = _performanceCounters[_nextPerformanceCounterEntry];
            _performanceCounters[_nextPerformanceCounterEntry] = pce;
            _nextPerformanceCounterEntry++;
            if (_nextPerformanceCounterEntry >= _numberOfPerformanceCounterSlots)
            {
                _nextPerformanceCounterEntry = 0;
            }
        }
        if (previous != nullptr)
        {
            if (previous->ownsMethod)
            {
                free(const_cast<char *>(previous->method));
            }
            Utils::DeleteAndNullifyPointer(previous);
        }
    }
    else
    {
        if (ownsMethod)
        {
            //
            //  The entry could not be stored, so nothing will ever release method through it;
            //  free it here instead so ownership is not dropped on this path.
            //
            free(const_cast<char *>(method));
        }
    }
}

/**
 *  @brief Mutex guarding _performanceCounters and _nextPerformanceCounterEntry.
 *
 *  A function-local static is created exactly once, on first use, even when several tasks
 *  get here at the same time.
 *
 *  @returns
 *      The performance counter mutex.
 */
SemaphoreHandle_t Logging::PerformanceCountersMutex()
{
    static const SemaphoreHandle_t mutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the performance counter mutex");
    return (mutex);
}

/**
 *  @brief Dump the table of performance counters.
 */
void Logging::DumpPerformanceCounters()
{
    //
    //  Held while the table is printed, so no entry is released part way through.
    //
    MutexLock lock(PerformanceCountersMutex());
    printf("Timestamp,Method,Duration\n");
    for (int index = 0; index < _numberOfPerformanceCounterSlots; index++)
    {
        if (_performanceCounters[index] != nullptr)
        {
            PerformanceCounterEntry *pce = _performanceCounters[index];
            printf("%" PRIu32 ",%s,%" PRIu32 "\n", pce->timestamp, pce->method, pce->duration);
        }
    }
}

#endif
