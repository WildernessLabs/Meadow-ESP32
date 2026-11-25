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
#include <utility>
#include "errno.h"

#include <esp_wifi.h>

#include "Mapping.hpp"
#include "SharedEnums.hpp"

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
    {.stm = 0, .esp = AF_UNSPEC},
    {.stm = 2, .esp = AF_INET},
    {.stm = 10, .esp = AF_INET6},
#if LWIP_IPV6
    {.stm = 10, .esp = AF_INET6} /* AF_INET6 */
#else                            /* LWIP_IPV6 */
    {.stm = 0, .esp = AF_INET6} /* AF_UNSPEC */
#endif
};

/**
 *  @brief Map the STM32 socket options to ESP32 socket options.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketOptions[] = {{.stm = 0, .esp = SO_ACCEPTCONN}, {.stm = 1, .esp = SO_BROADCAST}, {.stm = 2, .esp = SO_DEBUG},     {.stm = 3, .esp = SO_DONTROUTE}, {.stm = 4, .esp = SO_ERROR},     {.stm = 5, .esp = SO_KEEPALIVE},
                                                    {.stm = 6, .esp = SO_LINGER},     {.stm = 7, .esp = SO_OOBINLINE}, {.stm = 8, .esp = SO_RCVBUF},    {.stm = 9, .esp = SO_RCVLOWAT},  {.stm = 10, .esp = SO_RCVTIMEO}, {.stm = 11, .esp = SO_REUSEADDR},
                                                    {.stm = 12, .esp = SO_SNDBUF},    {.stm = 13, .esp = SO_SNDLOWAT}, {.stm = 14, .esp = SO_SNDTIMEO}, {.stm = 15, .esp = SO_TYPE}};

/**
 *  @brief Map the STM32 TCP options to ESP32 TCP options.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_tcpOptions[] = {{.stm = 0x10, .esp = TCP_NODELAY}, {.stm = 0x11, .esp = TCP_KEEPIDLE}, {.stm = 0x12, .esp = TCP_KEEPINTVL}, {.stm = 0x13, .esp = TCP_KEEPCNT}};

/**
 *  @brief Map the STM32 protocol family codes to ESP32 protocol family code.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_protocolFamilies[] = {{.stm = 2, .esp = PF_INET}, {.stm = 10, .esp = PF_INET6}, {.stm = 0, .esp = PF_UNSPEC}};

/**
 *  @brief Map the STM32 socket types to ESP32 socket types.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketTypes[] = {{.stm = 1, .esp = SOCK_STREAM}, {.stm = 2, .esp = SOCK_DGRAM}, {.stm = 3, .esp = SOCK_RAW}};

/**
 *  @brief Map the STM32 socket option levels to ESP32 socket option levels.
 *
 *  STM value is first, ESP value second.  The STM values are the NuttX 7
 *  levels, which are the wire format for both Meadow.OS versions: SOL_SOCKET 0,
 *  SOL_IP 1, SOL_TCP 3 and SOL_UDP 4, with IPPROTO_TCP (6) and IPPROTO_UDP (17)
 *  also accepted as levels.  NuttX 12 renumbered the levels (SOL_SOCKET 1,
 *  SOL_IP 0), and the same number means different levels in the two versions,
 *  so a NuttX 12 OS converts its levels to these values before sending them
 *  (espcp_mapping.c in Meadow.OS).
 */
Mapping::int_mapping_t Mapping::_socketLevels[] = {{.stm = 0, .esp = SOL_SOCKET}, {.stm = 1, .esp = IPPROTO_IP}, {.stm = 3, .esp = IPPROTO_TCP}, {.stm = 4, .esp = IPPROTO_UDP}, {.stm = 6, .esp = IPPROTO_TCP}, {.stm = 17, .esp = IPPROTO_UDP}};

/**
 *  @brief Map the STM32 error codes to ESP32 error codes.
 *
 *  STM value is first, ESP value second.  The STM values are the wire format
 *  for both Meadow.OS versions: the NuttX 7 errno numbering, which is the same
 *  as newlib's, so this table is the identity for every value it lists.  NuttX
 *  12 renumbered errno (upstream 153060d34dd), so a NuttX 12 OS converts these
 *  values to its own when it receives them (espcp_mapping.c in Meadow.OS).
 */
Mapping::int_mapping_t Mapping::_errno[] = {
    {.stm = 1, .esp = EPERM},
    {.stm = 2, .esp = ENOENT},
    {.stm = 3, .esp = ESRCH},
    {.stm = 4, .esp = EINTR},
    {.stm = 5, .esp = EIO},
    {.stm = 6, .esp = ENXIO},
    {.stm = 7, .esp = E2BIG},
    {.stm = 8, .esp = ENOEXEC},
    {.stm = 9, .esp = EBADF},
    {.stm = 10, .esp = ECHILD},
    {.stm = 11, .esp = EAGAIN},
    {.stm = 11, .esp = EWOULDBLOCK},
    {.stm = 12, .esp = ENOMEM},
    {.stm = 13, .esp = EACCES},
    {.stm = 14, .esp = EFAULT},
    //    {.stm = 15, .esp = ENOTBLK},
    {.stm = 16, .esp = EBUSY},
    {.stm = 17, .esp = EEXIST},
    {.stm = 18, .esp = EXDEV},
    {.stm = 19, .esp = ENODEV},
    {.stm = 20, .esp = ENOTDIR},
    {.stm = 21, .esp = EISDIR},
    {.stm = 22, .esp = EINVAL},
    {.stm = 23, .esp = ENFILE},
    {.stm = 24, .esp = EMFILE},
    {.stm = 25, .esp = ENOTTY},
    {.stm = 26, .esp = ETXTBSY},
    {.stm = 27, .esp = EFBIG},
    {.stm = 28, .esp = ENOSPC},
    {.stm = 29, .esp = ESPIPE},
    {.stm = 30, .esp = EROFS},
    {.stm = 31, .esp = EMLINK},
    {.stm = 32, .esp = EPIPE},
    {.stm = 33, .esp = EDOM},
    {.stm = 34, .esp = ERANGE},
    {.stm = 35, .esp = ENOMSG},
    {.stm = 36, .esp = EIDRM},
    /*
    {.stm = 37, .esp = ECHRNG},
    {.stm = 38, .esp = EL2NSYNC},
    {.stm = 39, .esp = EL3HLT},
    {.stm = 40, .esp = EL3RST},
    {.stm = 41, .esp = ELNRNG},
    {.stm = 42, .esp = EUNATCH},
    {.stm = 43, .esp = ENOCSI},
    {.stm = 44, .esp = EL2HLT},
    */
    {.stm = 45, .esp = EDEADLK},
    {.stm = 46, .esp = ENOLCK},
    /*
    {.stm = 50, .esp = EBADE},
    {.stm = 51, .esp = EBADR},
    {.stm = 52, .esp = EXFULL},
    {.stm = 53, .esp = ENOANO},
    {.stm = 54, .esp = EBADRQC},
    {.stm = 55, .esp = EBADSLT},
    {.stm = 56, .esp = EDEADLOCK},
    {.stm = 57, .esp = EBFONT},
    */
    {.stm = 60, .esp = ENOSTR},
    {.stm = 61, .esp = ENODATA},
    {.stm = 62, .esp = ETIME},
    {.stm = 63, .esp = ENOSR},
    {.stm = 64, .esp = ENOENT},
    /*
    {.stm = 65, .esp = ENOPKG},
    {.stm = 66, .esp = EREMOTE},
    */
    {.stm = 67, .esp = ENOLINK},
    /*
    {.stm = 68, .esp = EADV},
    {.stm = 69, .esp = ESRMNT},
    {.stm = 70, .esp = ECOMM},
    */
    {.stm = 71, .esp = EPROTO},
    {.stm = 74, .esp = EMULTIHOP},
    /*
    {.stm = 75, .esp = ELBIN},
    {.stm = 76, .esp = EDOTDOT},
    */
    {.stm = 77, .esp = EBADMSG},
    {.stm = 79, .esp = EFTYPE},
    /*
    {.stm = 80, .esp = ENOTUNIQ},
    {.stm = 81, .esp = EBADFD},
    {.stm = 82, .esp = EREMCHG},
    {.stm = 83, .esp = ELIBACC},
    {.stm = 84, .esp = ELIBBAD},
    {.stm = 85, .esp = ELIBSCN},
    {.stm = 86, .esp = ELIBMAX},
    {.stm = 87, .esp = ELIBEXEC},
    {.stm = 88, .esp = ENOSYS},
    {.stm = 89, .esp = ENMFILE},
    */
    {.stm = 90, .esp = ENOTEMPTY},
    {.stm = 91, .esp = ENAMETOOLONG},
    {.stm = 92, .esp = ELOOP},
    {.stm = 95, .esp = EOPNOTSUPP},
    {.stm = 96, .esp = EPFNOSUPPORT},
    {.stm = 104, .esp = ECONNRESET},
    {.stm = 105, .esp = ENOBUFS},
    {.stm = 106, .esp = EAFNOSUPPORT},
    {.stm = 107, .esp = EPROTOTYPE},
    {.stm = 108, .esp = ENOTSOCK},
    {.stm = 109, .esp = ENOPROTOOPT},
    {.stm = 110, .esp = ESHUTDOWN},
    {.stm = 111, .esp = ECONNREFUSED},
    {.stm = 112, .esp = EADDRINUSE},
    {.stm = 113, .esp = ECONNABORTED},
    {.stm = 114, .esp = ENETUNREACH},
    {.stm = 115, .esp = ENETDOWN},
    {.stm = 116, .esp = ETIMEDOUT},
    {.stm = 117, .esp = EHOSTDOWN},
    {.stm = 118, .esp = EHOSTUNREACH},
    {.stm = 119, .esp = EINPROGRESS},
    {.stm = 120, .esp = EALREADY},
    {.stm = 121, .esp = EDESTADDRREQ},
    {.stm = 122, .esp = EMSGSIZE},
    {.stm = 123, .esp = EPROTONOSUPPORT},
    // {.stm = 124, .esp = ESOCKTNOSUPPORT},
    {.stm = 125, .esp = EADDRNOTAVAIL},
    {.stm = 126, .esp = ENETRESET},
    {.stm = 127, .esp = EISCONN},
    {.stm = 128, .esp = ENOTCONN},
    {.stm = 129, .esp = ETOOMANYREFS},
    // {.stm = 130, .esp = EPROCLIM},
    // {.stm = 131, .esp = EUSERS},
    {.stm = 132, .esp = EDQUOT},
    {.stm = 133, .esp = ESTALE},
    {.stm = 134, .esp = ENOTSUP},
    // {.stm = 135, .esp = ENOMEDIUM},
    // {.stm = 136, .esp = ENOSHARE},
    // {.stm = 137, .esp = ECASECLASH},
    {.stm = 138, .esp = EILSEQ},
    {.stm = 139, .esp = EOVERFLOW},
    {.stm = 140, .esp = ECANCELED},
    {.stm = 141, .esp = ENOTRECOVERABLE},
    {.stm = 142, .esp = EOWNERDEAD}
    // {.stm = 143, .esp = ESTRPIPE}
};

/**
 *  @brief Map the STM32 message flags to ESP32 message flags.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_messageFlags[] = {
    {.stm = 0x0008, .esp = MSG_CTRUNC}, {.stm = 0x0040, .esp = MSG_DONTWAIT}, {.stm = 0x8000, .esp = MSG_MORE}, {.stm = 0x4000, .esp = MSG_NOSIGNAL}, {.stm = 0x0001, .esp = MSG_OOB}, {.stm = 0x0002, .esp = MSG_PEEK}, {.stm = 0x0020, .esp = MSG_TRUNC}, {.stm = 0x0100, .esp = MSG_WAITALL},
};

/**
 *  @brief Map the STM32 mono polling event flags to ESP32 polling event flags.
 *
 *  STM (Mono) value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_pollEvents[] = {{.stm = 0x01, .esp = POLLIN}, {.stm = 0x02, .esp = POLLPRI}, {.stm = 0x04, .esp = POLLOUT}, {.stm = 0x08, .esp = POLLERR}, {.stm = 0x10, .esp = POLLHUP}, {.stm = 0x20, .esp = POLLNVAL}};

/**
 *  @brief Map the STM32 socket shutdown types.
 *
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketShutdownType[] = {{.stm = 1, .esp = SHUT_RD}, {.stm = 2, .esp = SHUT_WR}, {.stm = 3, .esp = SHUT_RDWR}};

/**
 *  @brief Map of the ESP IDF error codes (esp_err_t) to StatusCodes.
 */
Mapping::int_mapping_t Mapping::_espErrorCodes[] = {
    {.stm = StatusCodes::CompletedOk, .esp = ESP_OK},
    {.stm = StatusCodes::EspOutOfMemory, .esp = ESP_ERR_NO_MEM},
    {.stm = StatusCodes::CannotConnectToAccessPoint, .esp = ESP_ERR_WIFI_CONN},
    {.stm = StatusCodes::EspWiFiInvalidSsid, .esp = ESP_ERR_WIFI_SSID},
    {.stm = StatusCodes::EspWiFiNotStarted, .esp = ESP_ERR_WIFI_NOT_STARTED},
    {.stm = StatusCodes::EspWiFiNotStarted, .esp = ESP_ERR_WIFI_NOT_INIT},
};

/**
 *  @brief Map of the ESP32 authentication types to the NetworkAuthenticationType enum in Meadow.Core
 */
Mapping::int_mapping_t Mapping::_networkAuthenticationType[] = {
    {.stm = 0, .esp = WIFI_AUTH_OPEN},           {.stm = 1, .esp = WIFI_AUTH_WEP}, {.stm = 2, .esp = WIFI_AUTH_WPA_PSK}, {.stm = 3, .esp = WIFI_AUTH_WPA2_PSK}, {.stm = 4, .esp = WIFI_AUTH_WPA_WPA2_PSK}, {.stm = 5, .esp = WIFI_AUTH_WPA2_ENTERPRISE}, {.stm = 13, .esp = WIFI_AUTH_WPA3_PSK},
    {.stm = 14, .esp = WIFI_AUTH_WPA2_WPA3_PSK},
};

/**
 *  @brief Map of the status codes to their string representations.
 */
Mapping::str_mapping_t Mapping::_statusCodes[] = {
    {.key = StatusCodes::CompletedOk, .value = "CompletedOk"},
    {.key = StatusCodes::CrcError, .value = "CrcError"},
    {.key = StatusCodes::Restart, .value = "Restarting"},
    {.key = StatusCodes::Failure, .value = "Failure"},
    {.key = StatusCodes::InvalidInterface, .value = "InvalidInterface"},
    {.key = StatusCodes::QueueError, .value = "QueueError"},
    {.key = StatusCodes::Timeout, .value = "Timeout"},
    {.key = StatusCodes::InvalidPacket, .value = "InvalidPacket"},
    {.key = StatusCodes::InvalidHeader, .value = "InvalidHeader"},
    {.key = StatusCodes::UnexpectedData, .value = "UnexpectedData"},
    {.key = StatusCodes::MissingEndOfFrameMarker, .value = "MissingEndOfFrameMarker"},
    {.key = StatusCodes::HeaderBodyFieldMismatch, .value = "HeaderBodyFieldMismatch"},
    {.key = StatusCodes::WiFiAlreadyStarted, .value = "WiFiAlreadyStarted"},
    {.key = StatusCodes::InvalidWiFiCredentials, .value = "InvalidWiFiCredentials"},
    {.key = StatusCodes::WiFiDisconnected, .value = "WiFiDisconnected"},
    {.key = StatusCodes::CannotStartNetworkInterface, .value = "CannotStartNetworkInterface"},
    {.key = StatusCodes::CannotConnectToAccessPoint, .value = "CannotConnectToAccessPoint"},
    {.key = StatusCodes::InvalidAntennaData, .value = "InvalidAntennaData"},
    {.key = StatusCodes::InvalidAntennaValue, .value = "InvalidAntennaValue"},
    {.key = StatusCodes::NoMessagesWaiting, .value = "NoMessagesWaiting"},
    {.key = StatusCodes::CoprocessorNotResponding, .value = "CoprocessorNotResponding"},
    {.key = StatusCodes::EspWiFiNotStarted, .value = "EspWiFiNotStarted"},
    {.key = StatusCodes::EspOutOfMemory, .value = "EspOutOfMemory"},
    {.key = StatusCodes::EspWiFiInvalidSsid, .value = "EspWiFiInvalidSsid"},
    {.key = StatusCodes::AccessPointNotFound, .value = "AccessPointNotFound"},
    {.key = StatusCodes::BeaconTimeout, .value = "BeaconTimeout"},
    {.key = StatusCodes::AuthenticationFailed, .value = "AuthenticationFailed"},
    {.key = StatusCodes::AssociationFailed, .value = "AssociationFailed"},
    {.key = StatusCodes::HandshakeTimeout, .value = "HandshakeTimeout"},
    {.key = StatusCodes::ConnectionFailed, .value = "ConnectionFailed"},
    {.key = StatusCodes::ApTsfReset, .value = "ApTsfReset"},
    {.key = StatusCodes::UnmappedErrorCode, .value = "UnmappedErrorCode"},
    {.key = StatusCodes::UnknownConfigurationItem, .value = "UnknownConfigurationItem"},
    {.key = StatusCodes::CannotStartAccessPoint, .value = "CannotStartAccessPoint"},
    {.key = StatusCodes::DhcpConfigurationError, .value = "DhcpConfigurationError"},
    {.key = StatusCodes::AccessPointNotStarted, .value = "AccessPointNotStarted"},
    {.key = StatusCodes::AccessPointAlreadyStarted, .value = "AccessPointAlreadyStarted"},
    {.key = StatusCodes::FileNotFound, .value = "FileNotFound"},
    {.key = StatusCodes::InvalidRequest, .value = "InvalidRequest"},
    {.key = StatusCodes::BluetoothNotStarted, .value = "BluetoothNotStarted"},
    {.key = StatusCodes::BluetoothDeviceNotBonded, .value = "BluetoothDeviceNotBonded"},
    {.key = StatusCodes::NetworkCloseDeadlock, .value = "NetworkCloseDeadlock"},
    {.key = StatusCodes::NetworkPollDeadlock, .value = "NetworkPollDeadlock"},
    {.key = StatusCodes::NetworkSocketDeadlock, .value = "NetworkSocketDeadlock"},
};

/**
 *  @brief Table of reset codes.
 */
Mapping::int_mapping_t Mapping::_resetReasons[] = {
    {.stm = Esp32ResetCodes::Unknown, .esp = ESP_RST_UNKNOWN},       {.stm = Esp32ResetCodes::PowerOn, .esp = ESP_RST_POWERON},   {.stm = Esp32ResetCodes::ExternalGpio, .esp = ESP_RST_EXT},
    {.stm = Esp32ResetCodes::Software, .esp = ESP_RST_SW},           {.stm = Esp32ResetCodes::Panic, .esp = ESP_RST_PANIC},       {.stm = Esp32ResetCodes::InterruptWatchdog, .esp = ESP_RST_INT_WDT},
    {.stm = Esp32ResetCodes::TaskWatchdog, .esp = ESP_RST_TASK_WDT}, {.stm = Esp32ResetCodes::OtherWatchdog, .esp = ESP_RST_WDT}, {.stm = Esp32ResetCodes::DeepSleep, .esp = ESP_RST_DEEPSLEEP},
    {.stm = Esp32ResetCodes::Brownout, .esp = ESP_RST_BROWNOUT},     {.stm = Esp32ResetCodes::SDIO, .esp = ESP_RST_SDIO},
};

/**
 *  @brief Map of the message types to their string representations.
 */
Mapping::str_mapping_t Mapping::_messageTypeNames[] = {
    {.key = MessageTypes::Ack, .value = "Ack"},        {.key = MessageTypes::Nak, .value = "Nak"},           {.key = MessageTypes::Reset, .value = "Reset"},
    {.key = MessageTypes::Event, .value = "Event"},    {.key = MessageTypes::Response, .value = "Response"}, {.key = MessageTypes::Transport, .value = "Transport"},
    {.key = MessageTypes::Header, .value = "Request"}, {.key = MessageTypes::Data, .value = "Data"},
};

/**
 *  @brief Map if the interface IDs to their names.
 */
Mapping::str_mapping_t Mapping::_interfaceNames[] = {
    {.key = Esp32Interfaces::WiFi, .value = "WiFi"}, {.key = Esp32Interfaces::BlueTooth, .value = "Bluetooth"}, {.key = Esp32Interfaces::MeshNetwork, .value = "MeshNetwork"}, {.key = Esp32Interfaces::System, .value = "System"}, {.key = Esp32Interfaces::Transport, .value = "Transport"},
};

/**
 *  @brief Map of the system function IDs to their names.
 */
Mapping::str_mapping_t Mapping::_systemFunctionNames[] = {
    {.key = SystemFunction::GetConfiguration, .value = "GetConfiguration"},
    {.key = SystemFunction::SetConfigurationItem, .value = "SetConfigurationItem"},
    {.key = SystemFunction::DeepSleep, .value = "DeepSleep"},
    {.key = SystemFunction::GetBatteryChargeLevel, .value = "GetBatteryChargeLevel"},
    {.key = SystemFunction::StartHeapTrace, .value = "StartHeapTrace"},
    {.key = SystemFunction::StopHeapTrace, .value = "StopHeapTrace"},
    {.key = SystemFunction::FileSystemFormat, .value = "FileSystemFormat"},
    {.key = SystemFunction::FileSystemListFiles, .value = "FileSystemListFiles"},
    {.key = SystemFunction::FileSystemWriteFile, .value = "FileSystemWriteFile"},
    {.key = SystemFunction::FileSystemReadFile, .value = "FileSystemReadFile"},
    {.key = SystemFunction::FileSystemDeleteFile, .value = "FileSystemDeleteFile"},
    {.key = SystemFunction::LoggingConfiguration, .value = "LoggingConfiguration"},
    {.key = SystemFunction::LogMessage, .value = "LogMessage"},
    {.key = SystemFunction::GetCoreDumpInformation, .value = "GetCoreDumpInformation"},
    {.key = SystemFunction::GetCoreDumpFragment, .value = "GetCoreDumpFragment"},
    {.key = SystemFunction::EraseCoreDump, .value = "EraseCoreDump"},
};

/**
 *  @brief Map of the bluetooth function IDs to their names.
 */
Mapping::str_mapping_t Mapping::_bluetoothFunctionNames[] = {
    {.key = BluetoothFunction::Start, .value = "Start"},
    {.key = BluetoothFunction::Stop, .value = "Stop"},
    {.key = BluetoothFunction::GetHandles, .value = "GetHandles"},
    {.key = BluetoothFunction::ServerDataSet, .value = "ServerDataSet"},
    {.key = BluetoothFunction::ClientWriteRequestEvent, .value = "ClientWriteRequestEvent"},
    {.key = BluetoothFunction::BluetoothStartingEvent, .value = "BluetoothStartingEvent"},
    {.key = BluetoothFunction::BluetoothStartedEvent, .value = "BluetoothStartedEvent"},
    {.key = BluetoothFunction::BluetoothStoppingEvent, .value = "BluetoothStoppingEvent"},
    {.key = BluetoothFunction::BluetoothStoppedEvent, .value = "BluetoothStoppedEvent"},
    {.key = BluetoothFunction::ClientConnectedEvent, .value = "ClientConnectedEvent"},
    {.key = BluetoothFunction::ClientDisconnectedEvent, .value = "ClientDisconnectedEvent"},
    {.key = BluetoothFunction::PasskeyReply, .value = "PasskeyReply"},
    {.key = BluetoothFunction::NumericComparisonReply, .value = "NumericComparisonReply"},
    {.key = BluetoothFunction::GetBondedDevices, .value = "GetBondedDevices"},
    {.key = BluetoothFunction::RemoveBond, .value = "RemoveBond"},
    {.key = BluetoothFunction::RemoveAllBonds, .value = "RemoveAllBonds"},
    {.key = BluetoothFunction::PasskeyDisplayEvent, .value = "PasskeyDisplayEvent"},
    {.key = BluetoothFunction::PasskeyRequestEvent, .value = "PasskeyRequestEvent"},
    {.key = BluetoothFunction::NumericComparisonRequestEvent, .value = "NumericComparisonRequestEvent"},
    {.key = BluetoothFunction::AuthenticationCompleteEvent, .value = "AuthenticationCompleteEvent"},
};

/**
 *  @brief Map of the transport function IDs to their names.
 */
Mapping::str_mapping_t Mapping::_transportFunctionNames[] = {
    {.key = TransportFunction::ResponseReady, .value = "ResponseReady"}, {.key = TransportFunction::SendResponse, .value = "SendResponse"}, {.key = TransportFunction::KillNuttxThread, .value = "KillNuttxThread"}, {.key = TransportFunction::ResetEsp32, .value = "ResetEsp32"}};

/**
 *  @brief Map of the WiFi function ID to their names.
 */
Mapping::str_mapping_t Mapping::_wifiFunctionNames[] = {
    {.key = WiFiFunction::ConnectToAccessPoint, .value = "ConnectToAccessPoint"},
    {.key = WiFiFunction::ConnectToDefaultAccessPoint, .value = "ConnectToDefaultAccessPoint"},
    {.key = WiFiFunction::StartAccessPoint, .value = "StartAccessPoint"},
    {.key = WiFiFunction::StopAccessPoint, .value = "StopAccesPoint"},
    {.key = WiFiFunction::NodeConnectedEvent, .value = "NodeConnected"},
    {.key = WiFiFunction::NodeDisconnectedEvent, .value = "NodeDisconnectedEvent"},
    {.key = WiFiFunction::DisconnectFromAccessPoint, .value = "DisconnectFromAccessPoint"},
    {.key = WiFiFunction::GetAccessPoints, .value = "GetAccessPoints"},
    {.key = WiFiFunction::GetAddrInfo, .value = "GetAddrInfo"},
    {.key = WiFiFunction::Socket, .value = "Socket"},
    {.key = WiFiFunction::Connect, .value = "Connect"},
    {.key = WiFiFunction::Write, .value = "Write"},
    {.key = WiFiFunction::SetSockOpt, .value = "SetSockOpt"},
    {.key = WiFiFunction::Read, .value = "Read"},
    {.key = WiFiFunction::Close, .value = "Close"},
    {.key = WiFiFunction::SendTo, .value = "SendTo"},
    {.key = WiFiFunction::RecvFrom, .value = "RecvFrom"},
    {.key = WiFiFunction::Poll, .value = "Poll"},
    {.key = WiFiFunction::InterruptPollResponse, .value = "InterruptPollResponse"},
    {.key = WiFiFunction::Send, .value = "Send"},
    {.key = WiFiFunction::Bind, .value = "Bind"},
    {.key = WiFiFunction::Listen, .value = "Listen"},
    {.key = WiFiFunction::Accept, .value = "Accept"},
    {.key = WiFiFunction::Ioctl, .value = "Ioctl"},
    {.key = WiFiFunction::GetSockName, .value = "GetSockName"},
    {.key = WiFiFunction::GetPeerName, .value = "GetPeerName"},
    {.key = WiFiFunction::FreeAddrInfo, .value = "FreeAddrInfo"},
    {.key = WiFiFunction::GetAddrInfo, .value = "GetAddrInfo"},
    {.key = WiFiFunction::RecvMsg, .value = "RecvMsg"},
    {.key = WiFiFunction::Shutdown, .value = "Shutdown"},
    {.key = WiFiFunction::SendMsg, .value = "SendMsg"},
    {.key = WiFiFunction::Dup2, .value = "Dup2"},
    {.key = WiFiFunction::AddRef, .value = "AddRef"},
    {.key = WiFiFunction::SockCaps, .value = "SockCaps"},
    {.key = WiFiFunction::StartWiFiInterfaceEvent, .value = "StartWiFiInterfaceEvent"},
    {.key = WiFiFunction::StopWiFiInterfaceEvent, .value = "StopWiFiInterfaceEvent"},
    {.key = WiFiFunction::NetworkConnectedEvent, .value = "NetworkConnectedEvent"},
    {.key = WiFiFunction::NetworkDisconnectedEvent, .value = "NetworkDisconnectedEvent"},
    {.key = WiFiFunction::NtpUpdateEvent, .value = "NtpUpdateEvent"},
    {.key = WiFiFunction::ErrorEvent, .value = "ErrorEvent"},
    {.key = WiFiFunction::StartAccessPoint, .value = "StartAccessPoint"},
    {.key = WiFiFunction::StopAccessPoint, .value = "StopAccessPoint"},
    {.key = WiFiFunction::AccessPointStartedEvent, .value = "AccessPointStartedEvent"},
    {.key = WiFiFunction::AccessPointStoppedEvent, .value = "AccessPointStoppedEvent"},
    {.key = WiFiFunction::NodeConnectedEvent, .value = "NodeConnectedEvent"},
    {.key = WiFiFunction::NodeDisconnectedEvent, .value = "NodeDisconnectedEvent"},
    {.key = WiFiFunction::NetworkConnectionRetryCountExceededEvent, .value = "NetworkConnectionRetryCountExceededEvent"},
    {.key = WiFiFunction::NetworkConnectingEvent, .value = "NetworkConnectingEvent"}};

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
Mapping::Mapping() = default;

/**
 *  @brief Default destructor for the mapping system.
 */
Mapping::~Mapping() = default;

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
    const auto bits = static_cast<uint32_t>(value);
    uint32_t flag = 0x0001U;
    uint32_t result = 0;

    while (flag != 0)
    {
        if ((bits & flag) != 0)
        {
            int32_t mappedFlag = 0;
            if (GetValue(mapping, type, static_cast<int32_t>(flag), &mappedFlag) == 0)
            {
                result |= static_cast<uint32_t>(mappedFlag);
            }
            else
            {
                return (-1);
            }
        }
        flag <<= 1U;
    }
    *mappedValue = static_cast<int32_t>(result);
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
 *  @param mappedValue
 *      Mapped value if successful.
 *
 *  @returns
 *      0 if the value could be found, -1 if there is a problem.
 */
int32_t Mapping::GetFlagValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int16_t * const mappedValue)
{
    int32_t mappedFlags;
    int32_t mappingResult = GetFlagValue(mapping, type, value, &mappedFlags);
    if (mappingResult == 0)
    {
        *mappedValue = static_cast<int16_t>(static_cast<uint32_t>(mappedFlags) & 0xffffU);
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
int32_t Mapping::GetValue(const MappingTable mapping, const MappingDirection type, const int32_t value, int8_t * const result)
{
    int32_t mappedValue;
    int32_t mappingResult = GetValue(mapping, type, value, &mappedValue);
    if (mappingResult == 0)
    {
        *result = static_cast<int8_t>(static_cast<uint32_t>(mappedValue) & 0xffU);
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
        *result = static_cast<int16_t>(static_cast<uint32_t>(mappedValue) & 0xffffU);
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
 * @note It is recognised that this method is not the most efficient way to do this, but it is the simplest and
 *       easiest to maintain as it does not place any restrictions on the ordering of the elements in the tables.
 *       If performance becomes an issue, then a more efficient method can be implemented.
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
    int_mapping_t *table = nullptr;
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

    if (table != nullptr)
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
    return (GetValue(mapping, Mapping::EspToStm, value, result));
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
    return (GetValue(mapping, Mapping::EspToStm, value, result));
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
int32_t Mapping::GetStmFlagValue(const MappingTable mapping, const int32_t value, int16_t * const result)
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
 *      String representing the specified value, nullptr if the value
 *      could not be found.
 */
const char *Mapping::GetStringValue(const Mapping::StringMappingTable mapping, const int32_t key)
{
    str_mapping_t *table = nullptr;
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

    if (table != nullptr)
    {
        for (size_t index = 0; index < size; index++)
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
