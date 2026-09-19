/**
 * @file Mapping.cpp
 * @author Mark Stevens
 * @brief Define the class that will allow the mapping of values on the STM32 to those on the ESP32 and vice-versa.
 * @date 2020-05-19
 *
 * @copyright Copyright (c) 2020 Wilderness Labs
 */

#include "sdkconfig.h"

#include <sys/socket.h>
#include "errno.h"

#include <esp_wifi.h>

#include "Encoders.hpp"
#include "Mapping.hpp"
#include "SharedEnums.hpp"
#include "SystemRequestHandler.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                     Initialise static data.
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  Static string to be used if a string mapping cannot be found.
 */
static const char *_unknown = "Unknown";

/**
 *  @brief Map the STM32 address family codes to ESP32 address family code.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_addressFamilies[] = {
    {1, AF_UNSPEC},
    {2, AF_INET},
    {10, AF_INET6},
#if LWIP_IPV6
    {10, AF_INET6} /* AF_INET6 */
#else              /* LWIP_IPV6 */
    {1, AF_INET6} /* AF_UNSPEC */
#endif
};

/**
 *  @brief Map the STM32 socket options to ESP32 socket options.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketOptions[] = {{0, SO_ACCEPTCONN}, {1, SO_BROADCAST}, {2, SO_DEBUG},     {3, SO_DONTROUTE},  {4, SO_ERROR},   {5, SO_KEEPALIVE}, {6, SO_LINGER},    {7, SO_OOBINLINE},
                                                    {8, SO_RCVBUF},     {9, SO_RCVLOWAT},  {10, SO_RCVTIMEO}, {11, SO_REUSEADDR}, {12, SO_SNDBUF}, {13, SO_SNDLOWAT}, {14, SO_SNDTIMEO}, {15, SO_TYPE}};

/**
 *  @brief Map the STM32 TCP options to ESP32 TCP options.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_tcpOptions[] = {{0x10, TCP_NODELAY}, {0x11, TCP_KEEPIDLE}, {0x12, TCP_KEEPINTVL}, {0x13, TCP_KEEPCNT}};

/**
 *  @brief Map the STM32 protocol family codes to ESP32 protocol family code.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_protocolFamilies[] = {{2, PF_INET}, {10, PF_INET6}, {1, PF_UNSPEC}};

/**
 *  @brief Map the STM32 socket types to ESP32 socket types.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketTypes[] = {{1, SOCK_STREAM}, {2, SOCK_DGRAM}, {3, SOCK_RAW}};

/**
 *  @brief Map the STM32 socket types to ESP32 socket types.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketLevels[] = {{0, SOL_SOCKET}, {3, IPPROTO_TCP}, {6, IPPROTO_TCP}};

/**
 *  @brief Map the STM32 error codes to ESP32 error codes.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_errno[] = {
    {1, EPERM},
    {2, ENOENT},
    {3, ESRCH},
    {4, EINTR},
    {5, EIO},
    {6, ENXIO},
    {7, E2BIG},
    {8, ENOEXEC},
    {9, EBADF},
    {10, ECHILD},
    {11, EAGAIN},
    {11, EWOULDBLOCK},
    {12, ENOMEM},
    {13, EACCES},
    {14, EFAULT},
    //    { 15, ENOTBLK },
    {16, EBUSY},
    {17, EEXIST},
    {18, EXDEV},
    {19, ENODEV},
    {20, ENOTDIR},
    {21, EISDIR},
    {22, EINVAL},
    {23, ENFILE},
    {24, EMFILE},
    {25, ENOTTY},
    {26, ETXTBSY},
    {27, EFBIG},
    {28, ENOSPC},
    {29, ESPIPE},
    {30, EROFS},
    {31, EMLINK},
    {32, EPIPE},
    {33, EDOM},
    {34, ERANGE},
    {35, ENOMSG},
    {36, EIDRM},
    /*
    { 37, ECHRNG },
    { 38, EL2NSYNC },
    { 39, EL3HLT },
    { 40, EL3RST },
    { 41, ELNRNG },
    { 42, EUNATCH },
    { 43, ENOCSI },
    { 44, EL2HLT },
    */
    {45, EDEADLK},
    {46, ENOLCK},
    /*
    { 50, EBADE },
    { 51, EBADR },
    { 52, EXFULL },
    { 53, ENOANO },
    { 54, EBADRQC },
    { 55, EBADSLT },
    { 56, EDEADLOCK },
    { 57, EBFONT },
    */
    {60, ENOSTR},
    {61, ENODATA},
    {62, ETIME},
    {63, ENOSR},
    {64, ENOENT},
    /*
    { 65, ENOPKG },
    { 66, EREMOTE },
    */
    {67, ENOLINK},
    /*
    { 68, EADV },
    { 69, ESRMNT },
    { 70, ECOMM },
    */
    {71, EPROTO},
    {74, EMULTIHOP},
    /*
    { 75, ELBIN },
    { 76, EDOTDOT },
    */
    {77, EBADMSG},
    {79, EFTYPE},
    /*
    { 80, ENOTUNIQ },
    { 81, EBADFD },
    { 82, EREMCHG },
    { 83, ELIBACC },
    { 84, ELIBBAD },
    { 85, ELIBSCN },
    { 86, ELIBMAX },
    { 87, ELIBEXEC },
    { 88, ENOSYS },
    { 89, ENMFILE },
    */
    {90, ENOTEMPTY},
    {91, ENAMETOOLONG},
    {92, ELOOP},
    {95, EOPNOTSUPP},
    {96, EPFNOSUPPORT},
    {104, ECONNRESET},
    {105, ENOBUFS},
    {106, EAFNOSUPPORT},
    {107, EPROTOTYPE},
    {108, ENOTSOCK},
    {109, ENOPROTOOPT},
    {110, ESHUTDOWN},
    {111, ECONNREFUSED},
    {112, EADDRINUSE},
    {113, ECONNABORTED},
    {114, ENETUNREACH},
    {115, ENETDOWN},
    {116, ETIMEDOUT},
    {117, EHOSTDOWN},
    {118, EHOSTUNREACH},
    {119, EINPROGRESS},
    {120, EALREADY},
    {121, EDESTADDRREQ},
    {122, EMSGSIZE},
    {123, EPROTONOSUPPORT},
    // { 124, ESOCKTNOSUPPORT },
    {125, EADDRNOTAVAIL},
    {126, ENETRESET},
    {127, EISCONN},
    {128, ENOTCONN},
    {129, ETOOMANYREFS},
    // { 130, EPROCLIM },
    // { 131, EUSERS },
    {132, EDQUOT},
    {133, ESTALE},
    {134, ENOTSUP},
    // { 135, ENOMEDIUM },
    // { 136, ENOSHARE },
    // { 137, ECASECLASH },
    {138, EILSEQ},
    {139, EOVERFLOW},
    {140, ECANCELED},
    {141, ENOTRECOVERABLE},
    {142, EOWNERDEAD}
    // { 143, ESTRPIPE }
};

/**
 *  @brief Map the STM32 message flags to ESP32 message flags.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_messageFlags[] = {
    {0x0008, MSG_CTRUNC}, {0x0040, MSG_DONTWAIT}, {0x8000, MSG_MORE}, {0x4000, MSG_NOSIGNAL}, {0x0001, MSG_OOB}, {0x0002, MSG_PEEK}, {0x0020, MSG_TRUNC}, {0x0100, MSG_WAITALL},
};

/**
 *  @brief Map the STM32 mono polling event flags to ESP32 polling event flags.
 *
 *  STM (Mono) value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_pollEvents[] = {{0x01, POLLIN}, {0x02, POLLPRI}, {0x04, POLLOUT}, {0x08, POLLERR}, {0x10, POLLHUP}, {0x20, POLLNVAL}};

/**
 *  @brief Map the STM32 socket shutdown types.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketShutdownType[] = {{1, SHUT_RD}, {2, SHUT_WR}, {3, SHUT_RDWR}};

/**
 *  @brief Map of the ESP IDF error codes (esp_err_t) to StatusCodes.
 */
Mapping::int_mapping_t Mapping::_espErrorCodes[] = {
    {StatusCodes::CompletedOk, ESP_OK},
    {StatusCodes::EspOutOfMemory, ESP_ERR_NO_MEM},
    {StatusCodes::CannotConnectToAccessPoint, ESP_ERR_WIFI_CONN},
    {StatusCodes::EspWiFiInvalidSsid, ESP_ERR_WIFI_SSID},
    {StatusCodes::EspWiFiNotStarted, ESP_ERR_WIFI_NOT_STARTED},
    {StatusCodes::EspWiFiNotStarted, ESP_ERR_WIFI_NOT_INIT},
};

/**
 *  @brief Map of the ESP32 authentication types to the NetworkAuthenticationType enum in Meadow.Core
 */
Mapping::int_mapping_t Mapping::_networkAuthenticationType[] = {
    {0, WIFI_AUTH_OPEN}, {1, WIFI_AUTH_WEP}, {2, WIFI_AUTH_WPA_PSK}, {3, WIFI_AUTH_WPA2_PSK}, {4, WIFI_AUTH_WPA_WPA2_PSK}, {5, WIFI_AUTH_WPA2_ENTERPRISE}, {13, WIFI_AUTH_WPA3_PSK}, {14, WIFI_AUTH_WPA2_WPA3_PSK},
};

/**
 *  @brief Map of the status codes to their string representations.
 */
Mapping::str_mapping_t Mapping::_statusCodes[] = {
    {StatusCodes::CompletedOk, "CompletedOk"},
    {StatusCodes::CrcError, "CrcError"},
    {StatusCodes::Restart, "Restarting"},
    {StatusCodes::Failure, "Failure"},
    {StatusCodes::InvalidInterface, "InvalidInterface"},
    {StatusCodes::QueueError, "QueueError"},
    {StatusCodes::Timeout, "Timeout"},
    {StatusCodes::InvalidPacket, "InvalidPacket"},
    {StatusCodes::InvalidHeader, "InvalidHeader"},
    {StatusCodes::UnexpectedData, "UnexpectedData"},
    {StatusCodes::MissingEndOfFrameMarker, "MissingEndOfFrameMarker"},
    {StatusCodes::HeaderBodyFieldMismatch, "HeaderBodyFieldMismatch"},
    {StatusCodes::WiFiAlreadyStarted, "WiFiAlreadyStarted"},
    {StatusCodes::InvalidWiFiCredentials, "InvalidWiFiCredentials"},
    {StatusCodes::WiFiDisconnected, "WiFiDisconnected"},
    {StatusCodes::CannotStartNetworkInterface, "CannotStartNetworkInterface"},
    {StatusCodes::CannotConnectToAccessPoint, "CannotConnectToAccessPoint"},
    {StatusCodes::InvalidAntennaData, "InvalidAntennaData"},
    {StatusCodes::InvalidAntennaValue, "InvalidAntennaValue"},
    {StatusCodes::NoMessagesWaiting, "NoMessagesWaiting"},
    {StatusCodes::CoprocessorNotResponding, "CoprocessorNotResponding"},
    {StatusCodes::EspWiFiNotStarted, "EspWiFiNotStarted"},
    {StatusCodes::EspOutOfMemory, "EspOutOfMemory"},
    {StatusCodes::EspWiFiInvalidSsid, "EspWiFiInvalidSsid"},
    {StatusCodes::AccessPointNotFound, "AccessPointNotFound"},
    {StatusCodes::BeaconTimeout, "BeaconTimeout"},
    {StatusCodes::AuthenticationFailed, "AuthenticationFailed"},
    {StatusCodes::AssociationFailed, "AssociationFailed"},
    {StatusCodes::HandshakeTimeout, "HandshakeTimeout"},
    {StatusCodes::ConnectionFailed, "ConnectionFailed"},
    {StatusCodes::ApTsfReset, "ApTsfReset"},
    {StatusCodes::UnmappedErrorCode, "UnmappedErrorCode"},
    {StatusCodes::UnknownConfigurationItem, "UnknownConfigurationItem"},
    {StatusCodes::CannotStartAccessPoint, "CannotStartAccessPoint"},
    {StatusCodes::DhcpConfigurationError, "DhcpConfigurationError"},
    {StatusCodes::AccessPointNotStarted, "AccessPointNotStarted"},
    {StatusCodes::AccessPointAlreadyStarted, "AccessPointAlreadyStarted"},
    {StatusCodes::FileNotFound, "FileNotFound"},
    {StatusCodes::InvalidRequest, "InvalidRequest"},
};

/**
 *  @brief Table of reset codes.
 */
Mapping::int_mapping_t Mapping::_resetReasons[] = {
    {Esp32ResetCodes::Unknown, ESP_RST_UNKNOWN},       {Esp32ResetCodes::PowerOn, ESP_RST_POWERON},   {Esp32ResetCodes::ExternalGpio, ESP_RST_EXT},    {Esp32ResetCodes::Software, ESP_RST_SW},       {Esp32ResetCodes::Panic, ESP_RST_PANIC}, {Esp32ResetCodes::InterruptWatchdog, ESP_RST_INT_WDT},
    {Esp32ResetCodes::TaskWatchdog, ESP_RST_TASK_WDT}, {Esp32ResetCodes::OtherWatchdog, ESP_RST_WDT}, {Esp32ResetCodes::DeepSleep, ESP_RST_DEEPSLEEP}, {Esp32ResetCodes::Brownout, ESP_RST_BROWNOUT}, {Esp32ResetCodes::SDIO, ESP_RST_SDIO},
};

/**
 *  @brief Map of the message types to their string representations.
 */
Mapping::str_mapping_t Mapping::_messageTypeNames[] = {
    {MessageTypes::Ack, "Ack"}, {MessageTypes::Nak, "Nak"}, {MessageTypes::Reset, "Reset"}, {MessageTypes::Event, "Event"}, {MessageTypes::Response, "Response"}, {MessageTypes::Transport, "Transport"}, {MessageTypes::Header, "Request"}, {MessageTypes::Data, "Data"},
};

/**
 *  @brief Map if the interface IDs to their names.
 */
Mapping::str_mapping_t Mapping::_interfaceNames[] = {
    {Esp32Interfaces::WiFi, "WiFi"}, {Esp32Interfaces::BlueTooth, "Bluetooth"}, {Esp32Interfaces::MeshNetwork, "MeshNetwork"}, {Esp32Interfaces::System, "System"}, {Esp32Interfaces::Transport, "Transport"},
};

/**
 *  @brief Map of the system function IDs to their names.
 */
Mapping::str_mapping_t Mapping::_systemFunctionNames[] = {
    {SystemFunction::GetConfiguration, "GetConfiguration"},
    {SystemFunction::SetConfigurationItem, "SetConfigurationItem"},
    {SystemFunction::DeepSleep, "DeepSleep"},
    {SystemFunction::GetBatteryChargeLevel, "GetBatteryChargeLevel"},
    {SystemFunction::StartHeapTrace, "StartHeapTrace"},
    {SystemFunction::StopHeapTrace, "StopHeapTrace"},
    {SystemFunction::FileSystemFormat, "FileSystemFormat"},
    {SystemFunction::FileSystemListFiles, "FileSystemListFiles"},
    {SystemFunction::FileSystemWriteFile, "FileSystemWriteFile"},
    {SystemFunction::FileSystemReadFile, "FileSystemReadFile"},
    {SystemFunction::FileSystemDeleteFile, "FileSystemDeleteFile"},
    {SystemFunction::LoggingConfiguration, "LoggingConfiguration"},
    {SystemFunction::LogMessage, "LogMessage"},
    {SystemFunction::GetCoreDumpInformation, "GetCoreDumpInformation"},
    {SystemFunction::GetCoreDumpFragment, "GetCoreDumpFragment"},
    {SystemFunction::EraseCoreDump, "EraseCoreDump"},
};

/**
 *  @brief Map of the bluetooth function IDs to their names.
 */
Mapping::str_mapping_t Mapping::_bluetoothFunctionNames[] = {
    {BluetoothFunction::Start, "Start"},
    {BluetoothFunction::Stop, "Stop"},
    {BluetoothFunction::GetHandles, "GetHandles"},
    {BluetoothFunction::ServerDataSet, "ServerDataSet"},
    {BluetoothFunction::ClientWriteRequestEvent, "ClientWriteRequestEvent"},
    {BluetoothFunction::BluetoothStartingEvent, "BluetoothStartingEvent"},
    {BluetoothFunction::BluetoothStartedEvent, "BluetoothStartedEvent"},
    {BluetoothFunction::BluetoothStoppingEvent, "BluetoothStoppingEvent"},
    {BluetoothFunction::BluetoothStoppedEvent, "BluetoothStoppedEvent"},
    {BluetoothFunction::ClientConnectedEvent, "ClientConnectedEvent"},
    {BluetoothFunction::ClientDisconnectedEvent, "ClientDisconnectedEvent"},
};

/**
 *  @brief Map of the transport function IDs to their names.
 */
Mapping::str_mapping_t Mapping::_transportFunctionNames[] = {{TransportFunction::ResponseReady, "ResponseReady"}, {TransportFunction::SendResponse, "SendResponse"}, {TransportFunction::KillNuttxThread, "KillNuttxThread"}, {TransportFunction::ResetEsp32, "ResetEsp32"}};

/**
 *  @brief Map of the WiFi function ID to their names.
 */
Mapping::str_mapping_t Mapping::_wifiFunctionNames[] = {
    {WiFiFunction::ConnectToAccessPoint, "ConnectToAccessPoint"},
    {WiFiFunction::ConnectToDefaultAccessPoint, "ConnectToDefaultAccessPoint"},
    {WiFiFunction::StartAccessPoint, "StartAccessPoint"},
    {WiFiFunction::StopAccessPoint, "StopAccesPoint"},
    {WiFiFunction::NodeConnectedEvent, "NodeConnected"},
    {WiFiFunction::NodeDisconnectedEvent, "NodeDisconnectedEvent"},
    {WiFiFunction::DisconnectFromAccessPoint, "DisconnectFromAccessPoint"},
    {WiFiFunction::GetAccessPoints, "GetAccessPoints"},
    {WiFiFunction::GetAddrInfo, "GetAddrInfo"},
    {WiFiFunction::Socket, "Socket"},
    {WiFiFunction::Connect, "Connect"},
    {WiFiFunction::Write, "Write"},
    {WiFiFunction::SetSockOpt, "SetSockOpt"},
    {WiFiFunction::Read, "Read"},
    {WiFiFunction::Close, "Close"},
    {WiFiFunction::SendTo, "SendTo"},
    {WiFiFunction::RecvFrom, "RecvFrom"},
    {WiFiFunction::Poll, "Poll"},
    {WiFiFunction::InterruptPollResponse, "InterruptPollResponse"},
    {WiFiFunction::Send, "Send"},
    {WiFiFunction::Bind, "Bind"},
    {WiFiFunction::Listen, "Listen"},
    {WiFiFunction::Accept, "Accept"},
    {WiFiFunction::Ioctl, "Ioctl"},
    {WiFiFunction::GetSockName, "GetSockName"},
    {WiFiFunction::GetPeerName, "GetPeerName"},
    {WiFiFunction::FreeAddrInfo, "FreeAddrInfo"},
    {WiFiFunction::GetAddrInfo, "GetAddrInfo"},
    {WiFiFunction::RecvMsg, "RecvMsg"},
    {WiFiFunction::Shutdown, "Shutdown"},
    {WiFiFunction::SendMsg, "SendMsg"},
    {WiFiFunction::Dup2, "Dup2"},
    {WiFiFunction::AddRef, "AddRef"},
    {WiFiFunction::SockCaps, "SockCaps"},
    {WiFiFunction::StartWiFiInterfaceEvent, "StartWiFiInterfaceEvent"},
    {WiFiFunction::StopWiFiInterfaceEvent, "StopWiFiInterfaceEvent"},
    {WiFiFunction::NetworkConnectedEvent, "NetworkConnectedEvent"},
    {WiFiFunction::NetworkDisconnectedEvent, "NetworkDisconnectedEvent"},
    {WiFiFunction::NtpUpdateEvent, "NtpUpdateEvent"},
    {WiFiFunction::ErrorEvent, "ErrorEvent"},
    {WiFiFunction::StartAccessPoint, "StartAccessPoint"},
    {WiFiFunction::StopAccessPoint, "StopAccessPoint"},
    {WiFiFunction::AccessPointStartedEvent, "AccessPointStartedEvent"},
    {WiFiFunction::AccessPointStoppedEvent, "AccessPointStoppedEvent"},
    {WiFiFunction::NodeConnectedEvent, "NodeConnectedEvent"},
    {WiFiFunction::NodeDisconnectedEvent, "NodeDisconnectedEvent"},
    {WiFiFunction::NetworkConnectionRetryCountExceededEvent, "NetworkConnectionRetryCountExceededEvent"},
    {WiFiFunction::NetworkConnectingEvent, "NetworkConnectingEvent"}};

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the mapping system.
 */
Mapping::Mapping()
{
}

/**
 *  @brief Default destructor for the mapping system.
 */
Mapping::~Mapping()
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
 *  @brief Convert the specified value for the specified platform.
 *
 *  This method will take a STM constant and convert it into the corresponding ESP
 *  constant (or the reverse depending upon the requested direction).
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param type
 *      Direction of the mapping.
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Mapped value if successful.
 *
 *  @returns
 *      0 if the value could be found, -1 if there is a problem.
 */
int32_t Mapping::GetFlagValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int32_t * const mappedValue)
{
    int32_t flag = 0x0001;
    int32_t result = 0;

    while (flag != 0)
    {
        if (value & flag)
        {
            int32_t mappedFlag = 0;
            if (GetValue(mapping, type, flag, &mappedFlag) == 0)
            {
                result |= mappedFlag;
            }
            else
            {
                return (-1);
            }
        }
        flag <<= 1;
    }
    *mappedValue = result;
    return (0);
}

/**
 *  @brief Convert the specified flags for the specified platform.
 *
 *  This method will take a STM flags and convert it into the corresponding ESP
 *  flags (or the reverse depending upon the requested direction).
 *
 *  Note that this is different from mapping a value as the flags could be an ORed
 *  combination of values.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param type
 *      Direction of the mapping.
 *
 *  @param value
 *      Value to map.
 *
 *  @param result
 *      Mapped value if successful.
 *
 *  @returns
 *      0 if the value could be found, -1 if there is a problem.
 */
int32_t Mapping::GetValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int8_t * const result)
{
    int32_t mappedValue;
    int32_t mappingResult = GetValue(mapping, type, value, &mappedValue);
    if (mappingResult == 0)
    {
        *result = mappedValue & 0xff;
    }

    return (mappingResult);
}

/**
 *  @brief Convert the specified flags for the specified platform.
 *
 *  This method will take a STM flags and convert it into the corresponding ESP
 *  flags (or the reverse depending upon the requested direction).
 *
 *  Note that this is different from mapping a value as the flags could be an ORed
 *  combination of values.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param type
 *      Direction of the mapping.
 *
 *  @param value
 *      Value to map.
 *
 *  @param result
 *      Mapped value if successful.
 *
 *  @returns
 *      0 if the value could be found, -1 if there is a problem.
 */
int32_t Mapping::GetValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int16_t * const result)
{
    int32_t mappedValue;
    int32_t mappingResult = GetValue(mapping, type, value, &mappedValue);
    if (mappingResult == 0)
    {
        *result = mappedValue & 0xffff;
    }

    return (mappingResult);
}

/**
 *  @brief Convert the specified flags for the specified platform.
 *
 *  This method will take a STM flags and convert it into the corresponding ESP
 *  flags (or the reverse depending upon the requested direction).
 *
 *  Note that this is different from mapping a value as the flags could be an ORed
 *  combination of values.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param type
 *      Direction of the mapping.
 *
 *  @param value
 *      Value to map.
 *
 *  @param result
 *      Mapped value if successful.
 *
 *  @returns
 *      0 if the value could be found, -1 if there is a problem.
 */
int32_t Mapping::GetValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int32_t * const result)
{
    int_mapping_t *table = NULL;
    size_t size = 0;
    switch (mapping)
    {
        case MappingTable::AddressFamily:
            table = _addressFamilies;
            size = sizeof(_addressFamilies);
            break;
        case MappingTable::SocketOptions:
            table = _socketOptions;
            size = sizeof(_socketOptions);
            break;
        case MappingTable::ProtocolFamilies:
            table = _protocolFamilies;
            size = sizeof(_protocolFamilies);
            break;
        case MappingTable::SocketTypes:
            table = _socketTypes;
            size = sizeof(_socketTypes);
            break;
        case MappingTable::SocketLevel:
            table = _socketLevels;
            size = sizeof(_socketLevels);
            break;
        case MappingTable::TCPOptions:
            table = _tcpOptions;
            size = sizeof(_tcpOptions);
            break;
        case MappingTable::Errno:
            table = _errno;
            size = sizeof(_errno);
            break;
        case MappingTable::MessageFlags:
            table = _messageFlags;
            size = sizeof(_messageFlags);
            break;
        case MappingTable::PollEvents:
            table = _pollEvents;
            size = sizeof(_pollEvents);
            break;
        case MappingTable::SocketShutdownType:
            table = _socketShutdownType;
            size = sizeof(_socketShutdownType);
            break;
        case MappingTable::EspErrorCodes:
            table = _espErrorCodes;
            size = sizeof(_espErrorCodes);
            break;
        case MappingTable::NetworkAuthenticationType:
            table = _networkAuthenticationType;
            size = sizeof(_networkAuthenticationType);
            break;
        case MappingTable::ResetReasons:
            table = _resetReasons;
            size = sizeof(_resetReasons);
            break;
    }
    size /= sizeof(int_mapping_t);

    if (table != NULL)
    {
        for (size_t index = 0; index < size; index++)
        {
            if (type == Mapping::StmToEsp)
            {
                if (table[index].stm == value)
                {
                    *result = table[index].esp;
                    return (0);
                }
            }
            else
            {
                if (table[index].esp == value)
                {
                    *result = table[index].stm;
                    return (0);
                }
            }
        }
    }
    return (-1);
}

/**
 *  @brief Get the ESP constant given the value of the STM constant.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Pointer to the memory location to hold the mapped value.
 *
 *  @returns
 *      0 if successful, -1 if there was an error.
 */
int32_t Mapping::GetEspValue(const MappingTable mapping, const int32_t value, int8_t * const result)
{
    return (GetValue(mapping, Mapping::StmToEsp, value, result));
}

/**
 *  @brief Get the ESP constant given the value of the STM constant.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Pointer to the memory location to hold the mapped value.
 *
 *  @returns
 *      0 if successful, -1 if there was an error.
 */
int32_t Mapping::GetEspValue(const MappingTable mapping, const int32_t value, int16_t * const result)
{
    return (GetValue(mapping, Mapping::StmToEsp, value, result));
}

/**
 *  @brief Get the ESP constant given the value of the STM constant.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Pointer to the memory location to hold the mapped value.
 *
 *  @returns
 *      0 if successful, -1 if there was an error.
 */
int32_t Mapping::GetEspValue(const MappingTable mapping, const int32_t value, int32_t * const result)
{
    return (GetValue(mapping, Mapping::StmToEsp, value, result));
}

/**
 *  @brief Get the STM constant given the specified ESP constant.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Pointer to the memory location to hold the mapped value.
 *
 *  @returns
 *      0 if successful, -1 if there was an error.
 */
int32_t Mapping::GetStmValue(const MappingTable mapping, const int32_t value, int8_t * const result)
{
    return (GetValue(mapping, Mapping::StmToEsp, value, result));
}

/**
 *  @brief Get the STM constant given the specified ESP constant.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Pointer to the memory location to hold the mapped value.
 *
 *  @returns
 *      0 if successful, -1 if there was an error.
 */
int32_t Mapping::GetStmValue(const MappingTable mapping, const int32_t value, int16_t * const result)
{
    return (GetValue(mapping, Mapping::StmToEsp, value, result));
}

/**
 *  @brief Get the STM constant given the specified ESP constant.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Pointer to the memory location to hold the mapped value.
 *
 *  @returns
 *      0 if successful, -1 if there was an error.
 */
int32_t Mapping::GetStmValue(const MappingTable mapping, const int32_t value, int32_t * const result)
{
    return (GetValue(mapping, Mapping::EspToStm, value, result));
}

/**
 *  @brief Get the ESP flags from the STM flags.
 *
 *  Note that the flags could be made up from multiple ORed flags.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Pointer to the memory location to hold the mapped value.
 *
 *  @returns
 *      0 if successful, -1 if there was an error.
 */
int32_t Mapping::GetEspFlagValue(const MappingTable mapping, const int32_t value, int32_t * const result)
{
    return (GetFlagValue(mapping, Mapping::StmToEsp, value, result));
}

/**
 *  @brief Get the STM flags from the ESP flags.
 *
 *  Note that the flags could be made up from multiple ORed flags.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @param result
 *      Pointer to the memory location to hold the mapped value.
 *
 *  @returns
 *      0 if successful, -1 if there was an error.
 */
int32_t Mapping::GetStmFlagValue(const MappingTable mapping, const int32_t value, int32_t * const result)
{
    return (GetFlagValue(mapping, Mapping::EspToStm, value, result));
}

/**
 *  @brief Map and ESP IDF error code into a StatusCodes value.
 *
 *  @param error
 *      ESP IDF error code (type esp_err_t).
 *
 *  @returns
 *      StatusCodes value equivalent to the ESP IDF error code or UnmappedErrorCode if the
 *      ESP value cannot be found.
 */
StatusCodes::StatusCodes Mapping::GetStatusCode(const esp_err_t error)
{
    int32_t result;
    if (GetStmValue(Mapping::EspErrorCodes, error, &result) < 0)
    {
        result = static_cast<int32_t>(StatusCodes::UnmappedErrorCode);
    }
    return (static_cast<StatusCodes::StatusCodes>(result));
}

/**
 *  @brief Get the string value represented by the key.
 *
 *  @param mapping
 *      Table to use for the translation.
 *
 *  @param key
 *      Value to look up.
 *
 *  @returns
 *      String representing the specified value, NULL if the value
 *      could not be found.
 */
const char *Mapping::GetStringValue(const Mapping::StringMappingTable mapping, const int32_t key)
{
    str_mapping_t *table = NULL;
    size_t size = 0;
    const char *result = _unknown;
    switch (mapping)
    {
        case StringMappingTable::StatusCodes:
            table = _statusCodes;
            size = sizeof(_statusCodes) / sizeof(str_mapping_t);
            break;
        case StringMappingTable::MessageTypes:
            table = _messageTypeNames;
            size = sizeof(_messageTypeNames) / sizeof(str_mapping_t);
            break;
        case StringMappingTable::Interfaces:
            table = _interfaceNames;
            size = sizeof(_interfaceNames) / sizeof(str_mapping_t);
            break;
        case StringMappingTable::BluetoothFunctions:
            table = _bluetoothFunctionNames;
            size = sizeof(_bluetoothFunctionNames) / sizeof(str_mapping_t);
            break;
        case StringMappingTable::SystemFunctions:
            table = _systemFunctionNames;
            size = sizeof(_systemFunctionNames) / sizeof(str_mapping_t);
            break;
        case StringMappingTable::TransportFunctions:
            table = _transportFunctionNames;
            size = sizeof(_transportFunctionNames) / sizeof(str_mapping_t);
            break;
        case StringMappingTable::WiFiFunctions:
            table = _wifiFunctionNames;
            size = sizeof(_wifiFunctionNames) / sizeof(str_mapping_t);
            break;
    }

    if (table != NULL)
    {
        for (int index = 0; index < size; index++)
        {
            if (key == table[index].key)
            {
                result = table[index].value;
                break;
            }
        }
    }

    return (result);
}

/**
 * @brief Map the ESP errno to a NuttX errno value.
 *
 * @param espErrno ESP IDF errno value.
 * @return int32_t NuttX errno value.
 */
int32_t Mapping::NuttxErrno(const int32_t espErrno)
{
    int32_t result = 0;
    if (GetStmValue(Mapping::Errno, espErrno, &result) != 0)
    {
        GetStmValue(Mapping::Errno, EFAULT, &result);
    }
    return (result);
}
