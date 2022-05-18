/*
 *  WiFiRequestHandler.cpp
 *
 *  Implementation of the WiFi functionality.
 */
#include "../build/config/sdkconfig.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lwip/apps/sntp.h"

#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "Mapping.hpp"
#include "Logging.hpp"

#if defined(CONFIG_HEAP_TRACING_STANDALONE) || defined(CONFIG_HEAP_TRACING_TOHOST)

#include "esp_heap_trace.h"

#endif

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

extern heap_trace_record_t *trace_records;

#endif

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

const char *WiFiRequestHandler::COMPONENT_NAME = "WiFiTask";

/**
 *  @brief Determine if this class is instantiated already (i.e. this should be a singleton).
 */
WiFiRequestHandler *WiFiRequestHandler::_instance = nullptr;

/**
 *  @brief Event group containing 24-bits used to indicate the state of the WiFi connection.
 */
EventGroupHandle_t WiFiRequestHandler::_xWiFiEventGroup = nullptr;

/**
 *  @brief List of active poll requests.
 */
// GenericLinkList<struct pollfd *> *WiFiRequestHandler::_pollRequests = nullptr;
std::map<uint32_t, struct pollfd *> WiFiRequestHandler::_pollRequests;

/**
 *  @brief Mutex for the poll request linked list.
 */
SemaphoreHandle_t WiFiRequestHandler::_pollRequestListMutex = NULL;

/**
 *  @brief Handle for the completed poll requests.
 */
QueueHandle_t WiFiRequestHandler::_completedPollRequestsQueue = NULL;

/**
 *  @brief GPIO Pin connected to the antenna selection switch
 *  which will connect the on board antenna to the ESP32.
 */
const gpio_num_t WiFiRequestHandler::ON_BOARD_ANTENNA_PIN = GPIO_NUM_26;

/**
 *  @brief GPIO Pin connected to the antenna selection switch
 *  which will connect the external antenna to the ESP32.
 */
const gpio_num_t WiFiRequestHandler::EXTERNAL_ANTENNA_PIN = GPIO_NUM_27;

/**
 *  @brief Index of the dummy socket file descriptor in poll request.
 */
const int WiFiRequestHandler::POLL_DUMMY_FD_INDEX = 1;

/**
 *  @brief Index of the actual socket file descriptor in poll request.
 */
const int WiFiRequestHandler::POLL_FD_INDEX = 0;

/**
 *  @brief Network Interface Handle for the board when running in station mode i.e. as a device connected to an access point.
 */
esp_netif_t *WiFiRequestHandler::_stationNetIfHandle;

/**
 *  @brief Network Interface Handle for the board when it is acting as a soft access point.
 */
esp_netif_t *WiFiRequestHandler::_softApNetIfHandle;
/**
 *  @brief Name of the NVS storage holding the current antenna value.
 */
const char *WiFiRequestHandler::ANTENNA_TYPE_NAME = "Antenna";

/**
 *  @brief Should the system automatically reconnect to an access point should there
 *  be a problem and the current connection is dropped.
 */
bool WiFiRequestHandler::_automaticReconnect = false;

/**
 *  @brief Name of the NVS storage holding the _automaticReconnect value.
 */
const char *WiFiRequestHandler::AUTOMATIC_RECONNECT_NAME = "AutoReconnect";

/**
 *  @brief Maximum number of times a failed event should retry before raising an error.
 */
uint32_t WiFiRequestHandler::_maximumRetryCount = 3;

/**
 *  @brief Name of the NVS storage holding the _maximumRetryCount value.
 */
const char *WiFiRequestHandler::MAXIMUM_RETRY_COUNT_NAME = "MaxRetry";

/**
 *  @brief Indicate if the network should start automatically at start up.
 */
bool WiFiRequestHandler:: _automaticallyStartNetwork = false;

/**
 *  @brief Name of the NVS storage holding the _automaticallyStartNetwork value.
 */
const char *WiFiRequestHandler::AUTOMATICALLY_START_NETWORK_NAME = "AutoStartNet";

/**
 *  @brief Indicate if the system should connect to a time server and get the
 *  current time at startup.
 */
bool WiFiRequestHandler::_getTimeAtStartup = false;

/**
 *  @brief Name fo the storage in NVS that holds the value indicating that the
 *  system should get the time at startup.
 */
const char *WiFiRequestHandler::GET_TIME_AT_STARTUP_NAME = "GetTime";

/**
 *  @brief Indicate if we should use DHCP.
 */
bool WiFiRequestHandler::_useDhcp = false;

/**
 *  @brief Name of the storage holding the _useDhcp configuration setting.
 */
const char *WiFiRequestHandler::USE_DHCP_NAME = "UseDHCP";

/**
 *  @brief Defaut access [oint to be used when automatically connecting.
 */
char *WiFiRequestHandler::_defaultAccessPoint = nullptr;

/**
 *  @brief Name of the storage in NVS holding the default access point.
 */
const char *WiFiRequestHandler::DEFAULT_ACCESS_POINT_NAME = "DefaultAP";

/**
 *  @brief Password for the access point.
 */
char *WiFiRequestHandler::_password = nullptr;

/**
 *  @brief Name of the storage in NVS holding the password for the access point.
 */
const char *WiFiRequestHandler::PASSWORD_NAME = "Password";

/**
 *  @brief Name of the NTP server to be used when using NTP to set the time
 *  when the board starts.
 */
char *WiFiRequestHandler::_ntpServer = (char *) "pool.ntp.org";

/**
 *  @brief Name of the storage space in NVS holding the NTP server information.
 */
const char *WiFiRequestHandler::NTP_SERVER_NAME = "NtpServer";

/**
 *  @brief Name of the storage space in NVS holding the timezone information.
 */
const char *WiFiRequestHandler::TIMEZONE_NAME = "Timezone";

/**
 *  @brief Timezone to be used when the ESP32 starts.  Default will be UTC.
 */
char *WiFiRequestHandler::_timezone = (char *) "UTC";

/**
 *  @brief Static IP address when not using DHCP.
 */
uint32_t WiFiRequestHandler::_staticIpAddress = 0;

/**
 *  @brief Name of the storage space holding the static IP address.
 */
const char *WiFiRequestHandler::STATIC_IP_ADDRESS_NAME = "IpAddress";

/**
 *  @brief DNS server when not using DHCP.
 */
uint32_t WiFiRequestHandler::_dnsServer = 0;

/**
 *  @brief Name of the storage in NVS holding the DNS server IP address.
 */
const char *WiFiRequestHandler::DNS_SERVER_NAME = "DnsServer";

/**
 *  @brief IP address of the default gateway when not using DHCP.
 */
uint32_t WiFiRequestHandler::_defaultGateway = 0;

/**
 *  @brief Name of the storage space holding the IP address of the default gateway.
 */
const char *WiFiRequestHandler::DEFAULT_GATEWAY_NAME = "Gateway";

/**
 *  @brief Default value for the WiFi timeout.
 */
uint32_t WiFiRequestHandler::_wifiTimeout = 30000;

/**
 *  @brief Name of the storage space holding the default WiFi timeout.
 */
const char *WiFiRequestHandler::DEFAULT_WIFI_TIMEOUT_NAME = "WiFiTimeout";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the WiFi class.
 */
WiFiRequestHandler::WiFiRequestHandler()
{
    if (_instance != nullptr)
    {
        throw new MultipleInstancesException();
    }
    else
    {
        _instance = this;
    }
    //
    //  Get configuration from NVS.
    //
    _defaultAccessPoint = SystemRequestHandler::GetConfigurationValue(DEFAULT_ACCESS_POINT_NAME, _defaultAccessPoint);
    _password = SystemRequestHandler::GetConfigurationValue(PASSWORD_NAME, _password);
    _stationNetIfHandle = nullptr;
    _softApNetIfHandle = nullptr;
    //
    //  Setup the timezones ready for when we get the time from a timeserver.
    //
    setenv("TZ", _timezone, 1);
    tzset();
    //
    //  Set up the structures need to deal with poll() requests in an asynchronous manner.
    //
    _pollRequestListMutex = xSemaphoreCreateMutex();
    if (_pollRequestListMutex == NULL)
    {
        ERROR_MESSAGE("WiFiRequestHandler: Cannot create poll request list mutex.");
    }

    //
    //  Finally set some default values for private fields.
    //
    ClearIpAndDisconnectData();
    bzero((void *) &_apInformation, sizeof(wifi_event_sta_connected_t));
    _lastIdfErrorCode = ESP_OK;
    _ipChanged = false;
}

/**
 *  @brief Create a new object and set the message dispatcher accordingly.
 *
 *  @param messageDispatcher
 *      Message Dispatcher object that can be used to send messages to
 *      the STM32.
 */
WiFiRequestHandler::WiFiRequestHandler(MessageDispatcher *messageDispatcher) : WiFiRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/**
 *  @brief Destructor for the WiFi class.
 *
 *  Free any resources used by the class.
 */
WiFiRequestHandler::~WiFiRequestHandler()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                      Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Get the automatic maximum retry count value.
 */
uint32_t WiFiRequestHandler::GetMaximumRetryCount()
{
    return(_maximumRetryCount == 0 ? 3 : _maximumRetryCount);
}

/**
 *  @brief Set the maximum retry count value.
 *
 *  @param value
 *      New maximum retry count value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetMaximumRetryCount(uint32_t value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetMaximumRetryCount: %d", value);
    _maximumRetryCount = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the automatic reconnect value.
 */
bool WiFiRequestHandler::GetAutomaticReconnect()
{
    return(_automaticReconnect);
}

/**
 *  @brief Set the automatic reconnect value.
 *
 *  @param value
 *      New automatic reconnection value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetAutomaticReconnect(bool value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetAutomaticReconnect: %d", value ? 1 : 0);
    _automaticReconnect = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the automatically start network value.
 */
bool WiFiRequestHandler::GetAutomaticallyStartNetwork()
{
    return(_automaticallyStartNetwork);
}

/**
 *  @brief Set the automatic start network value.
 *
 *  @param value
 *      New automatic start network value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetAutomaticallyStartNetwork(bool value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetAutomaticallyStartNetwork %d", value ? 1 : 0);
    _automaticallyStartNetwork = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the "get time at startup" value.
 */
bool WiFiRequestHandler::GetGetTimeAtStartup()
{
    return(_getTimeAtStartup);
}

/**
 *  @brief Set the "get time at startup" value.
 *
 *  @param value
 *      New get time at startup value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetGetTimeAtStartup(bool value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetGetTimeAtStartup %d", value ? 1 : 0);
    _getTimeAtStartup = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the use DHCP value.
 */
bool WiFiRequestHandler::GetUseDhcp()
{
    return(_useDhcp);
}

/**
 *  @brief Set the use DHCP value.
 *
 *  @param value
 *      New use DHCP value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetUseDhcp(bool value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetUseDhcp %d", value ? 1 : 0);
    _useDhcp = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the NTP server.
 */
char *WiFiRequestHandler::GetNtpServer()
{
    return(_ntpServer);
}

/**
 *  @brief Set NTP server.
 *
 *  @param value
 *      New NTP server value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetNtpServer(char *value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetNtpServer %s", value);
    _ntpServer = strdup(value);
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the timezone.
 */
char *WiFiRequestHandler::GetTimezone()
{
    return(_timezone);
}

/**
 *  @brief Set timezone.
 *
 *  @param value
 *      New NTP server value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetTimezone(char *value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetTimezone %s", value);
    _timezone = strdup(value);
    setenv("TZ", value, 1);
    tzset();
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the default access point.
 */
char *WiFiRequestHandler::GetDefaultAccessPoint()
{
    return(_defaultAccessPoint);
}

/**
 *  @brief Set default access point.
 *
 *  @param value
 *      New default access point value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetDefaultAccessPoint(char *value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetDefaultAccessPoint %s", (value == NULL) ? "NULL" : value);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    char emptyAp = '\0';

    if (value == NULL)
    {
        value = &emptyAp;
    }
    if (!_defaultAccessPoint)
    {
        _defaultAccessPoint = strdup(&emptyAp);
    }

    if (strcmp(value, _defaultAccessPoint) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(DEFAULT_ACCESS_POINT_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            free(_defaultAccessPoint);
            _defaultAccessPoint = strdup(value);
        }
    }
    return(result);
}

/**
 *  @brief Get the password for the default access point.
 */
char *WiFiRequestHandler::GetPassword()
{
    return(_password);
}

/**
 *  @brief Set password for the default access point.
 *
 *  @param value
 *      New password value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetPassword(char *value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetPassword %s", (value == NULL) ? "NULL" : value);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    char emptyPassword = '\0';

    if (value == NULL)
    {
        value = &emptyPassword;
    }
    if (!_password)
    {
        _password = strdup(&emptyPassword);
    }

    if (strcmp(value, _password) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(PASSWORD_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            free(_password);
            _password = strdup(value);
        }
    }
    return(result);
}

/**
 *  @brief Get the static IP address value.
 */
uint32_t WiFiRequestHandler::GetIpAddress()
{
    return(_staticIpAddress);
}

/**
 *  @brief Set the static IP address value.
 *
 *  @param value
 *      New static IP address value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetIpAddress(uint32_t value)
{
    _staticIpAddress = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the static DNS server value.
 */
uint32_t WiFiRequestHandler::GetDnsServer()
{
    return(_dnsServer);
}

/**
 *  @brief Set the static DNS address value.
 *
 *  @param value
 *      New static DNS address value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetDnsServer(uint32_t value)
{
    _dnsServer = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the static default gateway value.
 */
uint32_t WiFiRequestHandler::GetDefaultGateway()
{
    return(_defaultGateway);
}

/**
 *  @brief Set the static default gateway value.
 *
 *  @param value
 *      New static DNS address value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetDefaultGateway(uint32_t value)
{
    _defaultGateway = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the MAC address of the board.
 *
 *  @param macAddress
 *      Pointer to the buffer to hold the MAC address.
 *
 *  @param interface
 *      ESP interface to get the MAC address.  Valid values:
 *          WIFI_IF_STA     Board MAC address
 *          WIFI_IF_AP      Software access point MAC address
 */
void WiFiRequestHandler::GetMacAddress(uint8_t *macAddress, esp_interface_t interface)
{
    if (interface == WIFI_IF_STA)
    {
        if (esp_read_mac(macAddress, ESP_MAC_WIFI_STA) != ESP_OK)
        {
            bzero(macAddress, 6);
        }
    }
    else
    {
        if (esp_wifi_get_mac(WIFI_IF_AP, macAddress) != ESP_OK)
        {
            bzero(macAddress, 6);
        }
    }
}

/**
 *  @brief Set the MAC address of the board.
 *
 *  @param macAddress
 *      New MAC address.
 *
 *  @param interface
 *      Interface the MAC address refers to.  Valid values:
 *          WIFI_IF_STA     Board MAC address
 *          WIFI_IF_AP      Software access point MAC address
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetMacAddress(uint8_t *macAddress, esp_interface_t interface)
{
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the currently selected antenna.
 *
 *  @return
 *      Current Antenna in use as an unsigned 8-bit integer.
 */
uint8_t WiFiRequestHandler::GetAntenna()
{
    return(SystemRequestHandler::GetConfigurationValue(ANTENNA_TYPE_NAME, static_cast<uint8_t>(AntennaTypes::OnBoard)));
}

/**
 *  @brief Set the value for the current Antenna.
 *
 *  @param antenna
 *      New antenna to use.
 *
 *  TODO: Do we need to turn the WiFi off before switching the antenna?
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetAntenna(uint8_t antenna)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (antenna > static_cast<uint8_t>(AntennaTypes::Max))
    {
        result = StatusCodes::InvalidAntennaValue;
    }
    else
    {
        SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(antenna));
        SystemRequestHandler::SetConfigurationValue(ANTENNA_TYPE_NAME, antenna);
    }

    return(result);
}

/**
 *  @brief Get the WiFi timeout value (milliseconds).
 */
uint32_t WiFiRequestHandler::GetWiFiTimeout()
{
    return(_wifiTimeout);
}

/**
 *  @brief Set the WiFi timeout value (milliseconds).
 *
 *  @param value
 *      New WiFi timeout value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetWiFiTimeout(uint32_t value)
{
    _wifiTimeout = value;
    return(StatusCodes::CompletedOk);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Clear the IP and disconnect data.
 */
void WiFiRequestHandler::ClearIpAndDisconnectData()
{
    bzero((void *) &_instance->_disconnectData, sizeof(_disconnectData));
    bzero((void *) &_instance->_ipInformation, sizeof(_ipInformation));
}

/**
 *  @brief Map the errno variable from an ESP to a STM32 value.
 */
int32_t WiFiRequestHandler::MapErrno(int e)
{
    int32_t result = 0;
    if (Mapping::GetStmValue(Mapping::Errno, e, &result) != 0)
    {
        Mapping::GetStmValue(Mapping::Errno, EFAULT, &result);
    }
    return(result);
}

/**
 *  @brief Callback to be executed when the NTP time server has provided updated time information.
 *
 *  @param tv
 *      New time information from the time server.
 */
void WiFiRequestHandler::NtpServerCallback(struct timeval *tv)
{
    TRACE_MESSAGE("NtpServerCallback: Enter");

    TRACE_MESSAGE("Setting time to %d", (int) tv->tv_sec);

    settimeofday(tv, NULL);
    
    TRACE_MESSAGE("NtpServerCallback: Exit");
}

/**
 *  @brief Convert the antenna enum into a GPIO pin number
 *
 *  @param antenna
 *      Enum representing the antenna to be converted.
 *
 *  @returns
 *      GPIO pin number connected to the appropriate antenna.
 */
gpio_num_t WiFiRequestHandler::ConvertAntennaToPin(AntennaTypes::AntennaTypes antenna)
{
    TRACE_MESSAGE("ConvertAntennaToPin: Enter");
    TRACE_MESSAGE("ConvertAntennaToPin: Exit");

    return((antenna == AntennaTypes::OnBoard) ? WiFiRequestHandler::ON_BOARD_ANTENNA_PIN : WiFiRequestHandler::EXTERNAL_ANTENNA_PIN);
}

/**
 *  @brief Change the current antenna in use.
 *
 *  Note that this does not change the actual default antenna in use, it just switches to a new one.
 *
 *  @param newAntenna
 *      New antenna to use.
 */
void WiFiRequestHandler::SwitchAntenna(AntennaTypes::AntennaTypes newAntenna)
{
    TRACE_MESSAGE("SwitchAntenna: Enter");
    //
    // TODO: What happens to any pending requests and do we disconnect WiFi first?
    //
    gpio_num_t newAntennaPin = ConvertAntennaToPin(newAntenna);
    gpio_num_t oldAntennaPin = ConvertAntennaToPin(static_cast<AntennaTypes::AntennaTypes>(GetAntenna()));
    //
    //  Make sure the old antenna is switched off before the new one is turned on.
    //
    TRACE_MESSAGE("Switching from antenna %d to %d", oldAntennaPin, newAntennaPin);
    gpio_set_level(oldAntennaPin, 0);
    gpio_set_level(newAntennaPin, 1);
    TRACE_MESSAGE("SwitchAntenna; Exit");
}

/**
 *  @brief Setup the WiFi network class.
 */
void WiFiRequestHandler::Setup()
{
    TRACE_MESSAGE("Setup: Enter");
    RequestHandlerBase::Setup();
    gpio_set_direction(WiFiRequestHandler::EXTERNAL_ANTENNA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(WiFiRequestHandler::EXTERNAL_ANTENNA_PIN, 0);
    gpio_set_direction(WiFiRequestHandler::ON_BOARD_ANTENNA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(WiFiRequestHandler::ON_BOARD_ANTENNA_PIN, 1);

    SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(GetAntenna()));
    /*
     *  Now create the objects necessary for the WiFi tasks.
     */
    _xWiFiEventGroup = xEventGroupCreate();
    xEventGroupClearBits(_xWiFiEventGroup, 0x008fffff);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
    xTaskCreate(Task, COMPONENT_NAME, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
    TRACE_MESSAGE("Setup: Exit");
}

/**
 *  @brief Copy the IP and access point structure into the connection event data structure.
 *
 *  @param ed
 *    Pointer to the connection event data structure to be populated.
 */
void WiFiRequestHandler::PopulateConnectionEventDataStructure(Esp32Messaging::ConnectEventData *ed)
{
    TRACE_MESSAGE("PopulateConnectionEventDataStructure: Enter");

    bzero((void *) ed, sizeof(Esp32Messaging::ConnectEventData));
    //
    //  IP information.
    //
    memcpy((void *) &ed->IpAddress, (void *) &_ipInformation.ip.addr, sizeof(ed->IpAddress));
    memcpy((void *) &ed->SubnetMask, (void *) &_ipInformation.netmask.addr, sizeof(ed->SubnetMask));
    memcpy((void *) &ed->Gateway, (void *) &_ipInformation.gw.addr, sizeof(ed->Gateway));
    //
    //  Access point information.
    //
    memcpy((void *) &ed->Ssid, _apInformation.ssid, _apInformation.ssid_len);
    memcpy((void *) &ed->Bssid, _apInformation.bssid, sizeof(ed->Bssid));
    ed->Channel = _apInformation.channel;
    Mapping::GetStmValue(Mapping::NetworkAuthenticationType, _apInformation.authmode, (int8_t *) &ed->AuthenticationMode);

    TRACE_MESSAGE("PopulateConnectionEventDataStructure: Exit");
}

/**
 *  @brief Raise the ConnectToAccessPointEvent for the managed code.
 * 
 *  @param status
 *     Status code to send back to the STM32.
 */
void WiFiRequestHandler::RaiseConnectToAccessPointEvent(StatusCodes::StatusCodes status)
{
    TRACE_MESSAGE("RaiseConnectToAccessPointEvent: Enter");

    Esp32Messaging::ConnectEventData *ed = static_cast<Esp32Messaging::ConnectEventData *>( pvPortMalloc(sizeof(Esp32Messaging::ConnectEventData)));
    if (ed != NULL)
    {
        PopulateConnectionEventDataStructure(ed);
        uint32_t payloadLength = Encoders::EncodedConnectEventDataBufferSize(ed);
        uint8_t *payload = static_cast<uint8_t *> (pvPortMalloc(payloadLength));
        if (payload != NULL)
        {
            Encoders::EncodeConnectEventData(ed, payload);
            vPortFree(ed);
            RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::ConnectToAccessPointEvent, status, payload, payloadLength);
        }
        else
        {
            ERROR_MESSAGE("Failed to allocate memory for ConnectEventData payload.");
        }
    }
    else
    {
        ERROR_MESSAGE("Failed to allocate memory for ConnectEventData");
    }

    TRACE_MESSAGE("RaiseConnectToAccessPointEvent: Exit");
}

/**
 *  @brief Event loop for the IDF WiFi system.
 *
 *  Perform the actual event loop handling on behalf of the static method set up for the
 *  IDF event handler.
 *
 *  See the notes for the WiFiRequestHandler::EventHandler for more information as to why this
 *  method is necessary.
 *
 *  @param eventBase
 *      Subsystem that created the event.  This will be one of WIFI_EVENT or IP_EVENT.
 *
 *  @param eventId
 *      Type of event that has occurred.  This will vary depending upon the subsystem
 *      that generated the event.
 *
 *  @param eventData
 *      Data specific to the event that has occurred.
 */
void WiFiRequestHandler::EventHandlerHelper(esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    TRACE_MESSAGE("EventHandlerHelper: Enter");
    if (eventBase == WIFI_EVENT)
    {
        switch (eventId)
        {
            case WIFI_EVENT_WIFI_READY:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_WIFI_READY");
                break;
            case WIFI_EVENT_SCAN_DONE:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_SCAN_DONE");
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT);
                break;
            case WIFI_EVENT_STA_START:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_START");
                StationStartedEvent();
                break;
            case WIFI_EVENT_STA_STOP:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_STOP");
                StationStoppedEvent();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_CONNECTED");
                StationConnectedEvent((wifi_event_sta_connected_t *) eventData);
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_DISCONNECTED");
                StationDisconnectedEvent((wifi_event_sta_disconnected_t *) eventData);
                break;
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_AUTHMODE_CHANGE");
                break;
            case WIFI_EVENT_STA_WPS_ER_SUCCESS:
                TRACE_MESSAGE("EventHandlerHelper:  WIFI_EVENT_STA_WPS_ER_SUCCESS");
                break;
            case WIFI_EVENT_STA_WPS_ER_FAILED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_WPS_ER_FAILED");
                break;
            case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_WPS_ER_TIMEOUT");
                break;
            case WIFI_EVENT_STA_WPS_ER_PIN:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_WPS_ER_PIN");
                break;
            case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP");
                break;
            case WIFI_EVENT_AP_START:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_START");
                AccessPointStartedEvent();
                break;
            case WIFI_EVENT_AP_STOP:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_STOP");
                AccessPointStoppedEvent();
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_STACONNECTED");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_STADISCONNECTED");
                break;
            case WIFI_EVENT_AP_PROBEREQRECVED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_PROBEREQRECVED");
                break;
            default:
                TRACE_MESSAGE("EventHandlerHelper: Unhandled WiFi Event ID %d", eventId);
                break;
        }
    }
    else
    {
        if (eventBase == IP_EVENT)
        {
            switch (eventId)
            {
                case IP_EVENT_STA_GOT_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_STA_GOT_IP");
                    StationGotIpEvent((ip_event_got_ip_t *) eventData);
                    break;
                case IP_EVENT_STA_LOST_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_STA_LOST_IP");
                    break;
                case IP_EVENT_AP_STAIPASSIGNED:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_AP_STAIPASSIGNED");
                    break;
                case IP_EVENT_GOT_IP6:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_GOT_IP6");
                    break;
                case IP_EVENT_ETH_GOT_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_ETH_GOT_IP");
                    break;
                case IP_EVENT_PPP_GOT_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_PPP_GOT_IP");
                    break;
                case IP_EVENT_PPP_LOST_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_PPP_LOST_IP");
                    break;
                default:
                    TRACE_MESSAGE("EventHandlerHelper: Unhandled IP Event ID %d", eventId);
                    break;
            }
        }
    }
    TRACE_MESSAGE("EventHandlerHelper: Exit");
}

/**
 *  @brief Event loop for the IDF WiFi system.
 *
 *  Event loop (handler) for the WiFi and IP events as discussed in the following ESP-IDF document:
 *
 *  https://docs.espressif.com/projects/esp-idf/en/release-v4.1/api-reference/system/esp_event.html
 *
 *  This method passes the data on to an instance helper method.  This is done for convenience as it
 *  allows access to the member variables and methods without having to use the sender context or
 *  convert methods / variables / constants to static members.
 *
 *  @param arg
 *      Argument data specified when the event loop was first registered with
 *      esp_event_handler_register.  arg should point to the instance of the WiFiRequestHandler
 *      that registered the event loop.
 *
 *  @param eventBase
 *      Subsystem that created the event.  This will be one of WIFI_EVENT or IP_EVENT.
 *
 *  @param eventId
 *      Type of event that has occurred.  This will vary depending upon the subsystem
 *      that generated the event.
 *
 *  @param eventData
 *      Data specific to the event that has occurred.
 */
void WiFiRequestHandler::EventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    TRACE_MESSAGE("EventHandler: Enter");
    WiFiRequestHandler *sender = static_cast<WiFiRequestHandler *>(arg);
    sender->EventHandlerHelper(eventBase, eventId, eventData);
    TRACE_MESSAGE("EventHandler: Exit");
}

/**
 *  @brief Dispatch the WiFi request to the appropriate method in the class.
 *
 *  Some of the method calls in the switch statement below are followed by a call to
 *  xEventGroupSetBits and some are not.  The basic principle is that the POSIX calls
 *  that are executed through the thread pool need to have the
 *  WIFI_READY_FOR_NEXT_COMMAND_BIT set here as the thread pool does not know about
 *  the WiFi event group.  The other methods (in this class) have this set in the
 *  method call itself.
 *
 *  @param request
 *      Data structure containing the request information from the sender.
 */
void WiFiRequestHandler::DispatchRequest(Message *request)
{
    TRACE_MESSAGE("DispatchRequest: Enter");
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT | WIFI_ERROR_BIT);
    switch (request->Function)
    {
        case WiFiFunction::StartWiFiInterface:
            StartWiFiInterface(request);
            break;
        case WiFiFunction::StopWiFiInterface:
            StopWiFIAdapter(request);
            break;
        case WiFiFunction::ConnectToAccessPoint:
            ConnectToAccessPoint(request);
            break;
        case WiFiFunction::ConnectToDefaultAccessPoint:
            ConnectToDefaultAccessPoint(request);
            break;
        case WiFiFunction::DisconnectFromAccessPoint:
            DisconnectFromAccessPoint(request);
            break;
        case WiFiFunction::GetAccessPoints:
            GetAvailableNetworks(request);
            break;
        case WiFiFunction::ClearDefaultAccessPoint:
            ClearDefaultAccessPointAndPassword(request);
            break;
        case WiFiFunction::SetAntenna:
            SetAntennaRequest(request);
            break;
        case WiFiFunction::Close:
            SystemRequestHandler::SharedThreadPool()->Execute(Close, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Read:
            SystemRequestHandler::SharedThreadPool()->Execute(Read, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::SetSockOpt:
            SystemRequestHandler::SharedThreadPool()->Execute(SetSockOpt, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::GetSockOpt:
            SystemRequestHandler::SharedThreadPool()->Execute(GetSockOpt, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Write:
            SystemRequestHandler::SharedThreadPool()->Execute(Write, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::FreeAddrInfo:
            SystemRequestHandler::SharedThreadPool()->Execute(FreeAddrInfo, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Connect:
            SystemRequestHandler::SharedThreadPool()->Execute(Connect, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Socket:
            SystemRequestHandler::SharedThreadPool()->Execute(Socket, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::GetAddrInfo:
            SystemRequestHandler::SharedThreadPool()->Execute(GetAddrInfo, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::SendTo:
            SystemRequestHandler::SharedThreadPool()->Execute(SendTo, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::RecvFrom:
            SystemRequestHandler::SharedThreadPool()->Execute(RecvFrom, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Poll:
            Poll((void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Send:
            SystemRequestHandler::SharedThreadPool()->Execute(Send, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Accept:
            SystemRequestHandler::SharedThreadPool()->Execute(Accept, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Bind:
            SystemRequestHandler::SharedThreadPool()->Execute(Bind, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Listen:
            SystemRequestHandler::SharedThreadPool()->Execute(Listen, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Ioctl:
            SystemRequestHandler::SharedThreadPool()->Execute(Ioctl, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::GetSockName:
            SystemRequestHandler::SharedThreadPool()->Execute(GetSockName, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::GetPeerName:
            SystemRequestHandler::SharedThreadPool()->Execute(GetPeerName, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        default:
            TRACE_MESSAGE("DispatchRequest: Unknown WiFi message received, function %d", request->Function);
            if ((request->Payload != NULL) && (request->PayloadLength > 0))
            {
                TRACE_HEX_BUFFER(request->Payload, request->PayloadLength);
            }
            break;
    }
    xEventGroupWaitBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    TRACE_MESSAGE("DispatchRequest: Exit");
}

/**
 *  @brief Stop the WiFi adapter.
 */
StatusCodes::StatusCodes WiFiRequestHandler::StopWiFIAdapter()
{
    TRACE_MESSAGE("StopWiFIAdapter(): Enter");

    esp_err_t result = esp_wifi_stop();

    //
    //  Strictly speaking we do not need to unregister the event handlers as registering
    //  the handlers again will overwrite the previous handlers.
    //
    if (result == ESP_OK)
    {
        result = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiRequestHandler::EventHandler);
    }

    if (result == ESP_OK)
    {
        result = esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &WiFiRequestHandler::EventHandler);
    }

    if (result == ESP_OK)
    {
        result = esp_wifi_deinit();
    }

    if (result != ESP_OK)
    {
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
    }

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_INTERFACE_STARTING_BIT);

    TRACE_MESSAGE("StopWiFIAdapter(): Exit, ESP error code %d (%s)", result, esp_err_to_name(result));
    return(Mapping::GetStatusCode(result));
}

/**
 *  @brief Stop the WiFi adapter.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::StopWiFIAdapter(Message *message)
{
    TRACE_MESSAGE("StopWiFIAdapter(Message): Enter");

    bool raiseEvent = false;
    StatusCodes::StatusCodes sc = StatusCodes::CompletedOk;
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (bits & WIFI_CONNECTED_BIT)
    {
        sc = DisconnectFromAccessPoint();
    }
    if (sc == StatusCodes::CompletedOk)
    {
        sc = StopWiFIAdapter();
        if (sc == StatusCodes::CompletedOk)
        {
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_INTERFACE_STARTING_BIT);
            raiseEvent = true;
        }
        else
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
        }
    }
    else
    {
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
    }

    message->StatusCode = sc;
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    if (raiseEvent)
    {
        RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::StopWiFiInterfaceEvent, sc);
    }

    TRACE_MESSAGE("StopWiFIAdapter(Message): Exit");
}

/**
 *  @brief Disconnect from any access points.
 */
StatusCodes::StatusCodes WiFiRequestHandler::DisconnectFromAccessPoint()
{
    TRACE_MESSAGE("DisconnectFromAccessPoint(): Enter");
    StatusCodes::StatusCodes sc = StatusCodes::CompletedOk;

    esp_err_t result = esp_wifi_disconnect();
    if ((result == ESP_OK) || (result == ESP_ERR_WIFI_NOT_STARTED) || (result == ESP_ERR_WIFI_NOT_INIT))
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_RECONNECTING_BIT);
    }
    else
    {
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
        sc = StatusCodes::Failure;
    }
    ClearIpAndDisconnectData();

    TRACE_MESSAGE("DisconnectFromAccessPoint(): Exit");
    return(sc);
}

/**
 *  @brief Disconnect from the current access point.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::DisconnectFromAccessPoint(Message *message)
{
    TRACE_MESSAGE("DisconnectFromAccessPoint(Message): Enter");

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_DISCONNECT_REQUESTED_BIT);

    StatusCodes::StatusCodes sc = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        sc = DisconnectFromAccessPoint();
    }
    if (sc == StatusCodes::CompletedOk)
    {
        Esp32Messaging::DisconnectFromAccessPointRequest *request = Encoders::ExtractDisconnectFromAccessPointRequest(message->Payload);

        if (request->TurnOffWiFiInterface == 1)
        {
            sc = StopWiFIAdapter();
        }
        vPortFree(request);
    }

    vPortFree(message->Payload);
    message->Payload = NULL;
    message->PayloadLength = 0;

    message->StatusCode = sc;
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::DisconnectFromAccessPointEvent, sc);

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_DISCONNECT_REQUESTED_BIT);

    TRACE_MESSAGE("DisconnectFromAccessPoint(Message): Exit");
}

/**
 *  @brief Change the antenna.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::SetAntennaRequest(Message *message)
{
    TRACE_MESSAGE("SetAntennaRequest: Enter");

    Esp32Messaging::SetAntennaRequest *request = Encoders::ExtractSetAntennaRequest(message->Payload);
    vPortFree(message->Payload);
    message->Payload = nullptr;
    message->PayloadLength = 0;

    TRACE_MESSAGE("Switching antenna to %d with persist set to %d", request->Antenna, request->Persist);

    if (request->Persist)
    {
        SetAntenna(request->Antenna);   // This method also switches the antenna.
    }
    else
    {
        SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(request->Antenna));
    }

    vPortFree(request);
    message->StatusCode = StatusCodes::CompletedOk;
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("SetAntennaRequest: Exit");
}

/**
 *  @brief Indicate if we have a valid WiFi connection.
 *
 *  @returns
 *      True if the WiFi connection is active, false otherwise.
 */
bool WiFiRequestHandler::IsConnected()
{
    EventBits_t bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, 10 / portTICK_PERIOD_MS);

    return((bits & WIFI_CONNECTED_BIT) == WIFI_CONNECTED_BIT);
}

/**
 *  @brief Connect to a specified access point.
 *
 *  @param ssid
 *      SSID of the access point.
 *
 *  @param password
 *      Password for the access point.
 */
StatusCodes::StatusCodes WiFiRequestHandler::ConnectToAccessPoint(const char *ssid, const char *password)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (bits & WIFI_CONNECTED_BIT)
    {
        return(StatusCodes::WiFiAlreadyStarted);
    }

    if ((ssid == nullptr) ||(strlen(ssid) == 0))
    {
        return (StatusCodes::InvalidWiFiCredentials);
    }

    _currentRetryCount = 0;
    _lastIdfErrorCode = ESP_OK;
    ClearIpAndDisconnectData();
    if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    {
        if (StartWiFiInterface() != ESP_OK)
        {
            result = StatusCodes::CannotStartNetworkInterface;
        }
        else
        {
            TRACE_MESSAGE("WiFi interface started OK.");
        }
    }

    if (result == StatusCodes::CompletedOk)
    {
        _currentRetryCount = 0;
        char zeroLengthString[1] = { 0 };
        if (password == nullptr)
        {
            password = zeroLengthString;
        }
        if ((strlen(ssid) > MAXIMUM_SSID_LENGTH) || (strlen(password) > MAXIMUM_PASSWORD_LENGTH))
        {
            result = StatusCodes::InvalidWiFiCredentials;
            TRACE_MESSAGE("ConnectToAccessPoint: Invalid credentials");
        }
        else
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
            wifi_config_t wifi_config = { };
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config.sta.pmf_cfg =
            {
                .capable = true,
                .required = false
            };
            memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
            memcpy(wifi_config.sta.password, password, strlen(password));
            _lastIdfErrorCode = esp_wifi_set_mode(WIFI_MODE_STA);
            if (_lastIdfErrorCode == ESP_OK)
            {
                _lastIdfErrorCode = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
            }
            if (_lastIdfErrorCode == ESP_OK)
            {
                _lastIdfErrorCode = esp_wifi_start();
            }
            if (_lastIdfErrorCode == ESP_OK)
            {
                _lastIdfErrorCode = esp_wifi_connect();
            }
            if (_lastIdfErrorCode == ESP_OK)
            {
                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, 600000 / portTICK_PERIOD_MS);
                if (bits & WIFI_ERROR_BIT)
                {
                    result = StatusCodes::CannotConnectToAccessPoint;
                }
                else
                {
                    if (bits & WIFI_CONNECTED_BIT)
                    {
                        result = StatusCodes::CompletedOk;
                    }
                    else
                    {
                        result = StatusCodes::Timeout;
                    }
                }
            }
            else
            {
                int32_t found;
                result = (StatusCodes::StatusCodes) Mapping::GetStmValue(Mapping::EspErrorCodes, (int32_t) _lastIdfErrorCode, &found);
            }
        }
    }
    TRACE_MESSAGE("Result %d", result);
    return(result);
}

/**
 *  @brief Connect to a specified access point.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ConnectToAccessPoint(Message *message)
{
    TRACE_MESSAGE("ConnectToAccessPoint: Enter");
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    Esp32Messaging::WiFiCredentials *credentials = Encoders::ExtractWiFiCredentials(message->Payload);
    message->DeletePayload();

    result = ConnectToAccessPoint(credentials->NetworkName, credentials->Password);
    vPortFree(credentials);

    Esp32Messaging::ConnectEventData data = { };

    if (result == StatusCodes::CompletedOk)
    {
        PopulateConnectionEventDataStructure(&data);
    }
    else
    {
        memcpy((void *) data.Ssid, (void *) _disconnectData.ssid, _disconnectData.ssid_len);
        memcpy((void *) data.Bssid, (void *) _disconnectData.bssid, sizeof(data.Bssid));
        TRACE_MESSAGE("Failed to connect to access point, code: %d", _disconnectData.reason);
        data.Reason = _disconnectData.reason;
        ClearIpAndDisconnectData();
        _lastIdfErrorCode = ESP_OK;
    }
    message->PayloadLength = Encoders::EncodedConnectEventDataBufferSize(&data);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeConnectEventData(&data, message->Payload);

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    Logging::DumpMessage(COMPONENT_NAME, message);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    if (result == StatusCodes::CompletedOk)
    {
        RaiseConnectToAccessPointEvent(result);
    }

    TRACE_MESSAGE("ConnectToAccessPoint: Exit");
}

/**
 *  @brief Connect to the default access point.
 *
 *  Note: This is a non-blocking call and will generate the connection event
 *        with the status code set accordingly. 
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ConnectToDefaultAccessPoint(Message *message)
{
    TRACE_MESSAGE("ConnectToDefaultAccessPoint: Enter");
    StatusCodes::StatusCodes result = StatusCodes::DefaultAccessPointNotConfigured;

    char *access_point = GetDefaultAccessPoint();
    if (access_point != NULL)
    {
        result = ConnectToAccessPoint(access_point, GetPassword());
    }

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    if (result == StatusCodes::CompletedOk)
    {
        RaiseConnectToAccessPointEvent(result);
    }

    TRACE_MESSAGE("ConnectToDefaultAccessPoint: Exit");
}

/**
 *  @brief Clear the default access point and password from NVS.
 * 
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ClearDefaultAccessPointAndPassword(Message *message)
{
    TRACE_MESSAGE("ClearDefaultAccessPointAndPassword: Enter");

    message->MessageType = MessageTypes::Response;
    StatusCodes::StatusCodes sc = SetDefaultAccessPoint(NULL);
    StatusCodes::StatusCodes sc1 = SetPassword(NULL);
    message->StatusCode = (sc == StatusCodes::CompletedOk) && (sc1 == StatusCodes::CompletedOk) ? StatusCodes::CompletedOk : StatusCodes::Failure;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("ClearDefaultAccessPointAndPassword: Exit");
}

/**
 *  @brief Start the network interface.
 *
 *  Start the WiFi network interface ready for the device to be configured
 *  as either an access point (softAP) or as a network device (station).
 *
 *  @return
 *      ESP_OK if the interface is initialised correctly, an error code otherwise.
 */
esp_err_t WiFiRequestHandler::StartWiFiInterface()
{
    TRACE_MESSAGE("StartWiFiInterface: Enter");
    esp_err_t result = ESP_OK;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    {
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT);
        result = esp_netif_init();
        if (result == ESP_OK)
        {
            result = esp_event_loop_create_default();
        }

        if (result == ESP_OK)
        {
            _stationNetIfHandle = NULL;
            _stationNetIfHandle = esp_netif_create_default_wifi_sta();
            if (_stationNetIfHandle == NULL)
            {
                result = ESP_FAIL;
            }
        }

        if (result == ESP_OK)
        {
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            result = esp_wifi_init(&cfg);
        }

        if (result == ESP_OK)
        {
            result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiRequestHandler::EventHandler, this);
        }

        if (result == ESP_OK)
        {
            result = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &WiFiRequestHandler::EventHandler, this);
        }

        if (result == ESP_OK)
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT);
        }
        else
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
        }
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT);
    }
    TRACE_MESSAGE("StartWiFiInterface: Exit");
    return(result);
}

/**
 *  @brief Process the request to start the WiFi network interface.
 *
 *  This will only start the WiFi interface, it will not connect to an access
 *  point.  Also, this will only operate in station mode at the moment.
 *
 *  @param message
 *      Request sent from the STM32.
 */
void WiFiRequestHandler::StartWiFiInterface(Message *message)
{
    TRACE_MESSAGE("StartNetwork: Enter");
    message->MessageType = MessageTypes::Response;
    StatusCodes::StatusCodes sc = (StartWiFiInterface() == ESP_OK) ? StatusCodes::CompletedOk : StatusCodes::Failure;
    message->StatusCode = sc;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::StartWiFiInterfaceEvent, sc);

    TRACE_MESSAGE("StartNetwork: Exit");
}

/**
 *  @brief Get the full list of available networks.
 *
 *  @pre
 *      The network interfce must be started before this method is called.
 *      It is not necessary to connected to an access pint but the interface
 *      must be started.
 *
 *  @param message
 *      Request sent from the STM32.
 */
void WiFiRequestHandler::GetAvailableNetworks(Message *message)
{
    TRACE_MESSAGE("GetAvailableNetworks: Enter");
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    _currentRetryCount = 0;
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    {
        TRACE_MESSAGE("Starting network");
        if (StartWiFiInterface() != ESP_OK)
        {
            result = StatusCodes::CannotStartNetworkInterface;
        }
    }

    if (result == StatusCodes::CompletedOk)
    {
        //
        //  The maximum packet size is 4,000 bytes for SPI and a single access point contains 47 bytes
        //  of data (say 50) which gives space for about 80 access points in the packet.
        //
        #define MAX_AP 80
        uint16_t number = MAX_AP;
        wifi_ap_record_t *ap_info = (wifi_ap_record_t *) pvPortMalloc(MAX_AP * sizeof(wifi_ap_record_t));
        if (ap_info == NULL)
        {
            result = StatusCodes::Failure;
        }
        else
        {
            bzero(ap_info, MAX_AP * sizeof(wifi_ap_record_t));

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_start());
            bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
            if (bits & WIFI_INTERFACE_STARTED_BIT)
            {
                ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
                if (bits & WIFI_SCAN_COMPLETE_BIT)
                {
                    uint16_t apCount = 0;
                    esp_wifi_scan_get_ap_num(&apCount);
                    TRACE_MESSAGE("GetAvailableNetworks: Number of access points found: %d", apCount);
                    if (apCount != 0)
                    {
                        Esp32Messaging::AccessPointList *accessPointList = static_cast<Esp32Messaging::AccessPointList *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPointList)));
                        accessPointList->NumberOfAccessPoints = apCount;
                        //
                        //  Now we get the access point information.
                        //
                        wifi_ap_record_t *accessPoints = static_cast<wifi_ap_record_t *>(pvPortMalloc(apCount * sizeof(wifi_ap_record_t)));
                        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, accessPoints));

                        Esp32Messaging::AccessPoint *accessPoint = static_cast<Esp32Messaging::AccessPoint *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPoint)));
                        int singleAccessPointSize = Encoders::EncodedAccessPointBufferSize(accessPoint);
                        accessPointList->AccessPointsLength = singleAccessPointSize * apCount;
                        accessPointList->AccessPoints = static_cast<uint8_t *>(pvPortMalloc(accessPointList->AccessPointsLength));
                        uint8_t *currentLocation = accessPointList->AccessPoints;
                        for (int index = 0; index < apCount; index++)
                        {
                            bzero(accessPoint, sizeof(Esp32Messaging::AccessPoint));
                            memcpy(accessPoint->Ssid, &accessPoints[index].ssid, 33);
                            memcpy(accessPoint->Bssid, &accessPoints[index].bssid, 6);
                            accessPoint->PrimaryChannel = accessPoints[index].primary;
                            accessPoint->SecondaryChannel = accessPoints[index].second;
                            accessPoint->Rssi = accessPoints[index].rssi;
                            accessPoint->AuthenticationMode = accessPoints[index].authmode;
                            accessPoint->Protocols = accessPoints[index].phy_11b | accessPoints[index].phy_11g | accessPoints[index].phy_11n | accessPoints[index].phy_lr | accessPoints[index].wps;
                            Encoders::EncodeAccessPoint(accessPoint, currentLocation);
                            currentLocation += singleAccessPointSize;
                        }
                        vPortFree(accessPoints);
                        vPortFree(accessPoint);

                        message->PayloadLength = Encoders::EncodedAccessPointListBufferSize(accessPointList);
                        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
                        Encoders::EncodeAccessPointList(accessPointList, message->Payload);
                        vPortFree(accessPointList);
                    }
                    result = StatusCodes::CompletedOk;
                }
                else
                {
                    TRACE_MESSAGE("GetAvailableNetworks: Scan timeout.");
                    result = StatusCodes::Timeout;
                }
                vPortFree(ap_info);
            }
            else
            {
                TRACE_MESSAGE("GetAvailableNetworks: Scan timeout.");
                result = StatusCodes::Timeout;
            }
        }
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
    TRACE_MESSAGE("GetAvailableNetworks: Exit");
}

/**
 *  @brief Expose the POSIX getaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getaddrinfo method.
 */
void WiFiRequestHandler::GetAddrInfo(void *vpMessage)
{
    TRACE_MESSAGE("GetAddrInfo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::GetAddrInfoResponse *response = static_cast<Esp32Messaging::GetAddrInfoResponse *>(pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoResponse)));

    if (IsConnected())
    {
        Esp32Messaging::GetAddrInfoRequest *request = Encoders::ExtractGetAddrInfoRequest(message->Payload);
        //
        Esp32Messaging::AddrInfo *extractedHints = Encoders::ExtractAddrInfo(request->Hints);
        vPortFree(request->Hints);

        struct addrinfo hints = { };
        hints.ai_family = extractedHints->Family;
        hints.ai_socktype = extractedHints->SocketType;
        vPortFree(extractedHints);
        //
        //  This will hold a pointer to a chain of one or more addrinfo structure.
        //
        //  It is the responsibility of the calling .NET application to ensure
        //  that FreeAddrInfo is called.  FreeAddrInfo will free the allocated
        //  memory.
        //
        struct addrinfo *addressInformation;
        int errorCode = getaddrinfo(request->NodeName, request->ServName, &hints, &addressInformation);
        vPortFree(request);

        response->AddrInfoResponseErrno = MapErrno(errorCode);
        if ((errorCode == 0) || (addressInformation != NULL))
        {
            Esp32Messaging::SockAddr sa = { };
            Esp32Messaging::AddrInfo a = { };

            int family;
            Mapping::GetStmValue(Mapping::AddressFamily, addressInformation->ai_addr->sa_family, &family);
            sa.Family = (unsigned char) (family & 0xff);
            Encoders::EncodeSockAddr(&sa, a.Addr);

            a.CanonName = strdup(addressInformation->ai_canonname);
            a.Family = addressInformation->ai_family;
            a.Flags = addressInformation->ai_flags;
            a.MyHeapAddress = reinterpret_cast<uint32_t>(addressInformation);
            a.Next = addressInformation->ai_next;
            a.Protocol = addressInformation->ai_protocol;
            a.SocketType = addressInformation->ai_socktype;
            a.AddrLen = addressInformation->ai_addrlen;

            response->ResLength = Encoders::EncodedAddrInfoBufferSize(&a);
            response->Res = static_cast<uint8_t *>(pvPortMalloc(response->ResLength));
            Encoders::EncodeAddrInfo(&a, response->Res);
        }
        else
        {
            response->AddrInfoResponseErrno = MapErrno(errno);
            vPortFree(addressInformation);
        }
    }
    else
    {
        response->AddrInfoResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedGetAddrInfoResponseBufferSize(response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeGetAddrInfoResponse(response, message->Payload);
    vPortFree(response);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetAddrInfo: Exit");
}

/**
 *  @brief Expose the POSIX socket method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the socket method.
 */
void WiFiRequestHandler::Socket(void *vpMessage)
{
    TRACE_MESSAGE("Socket: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::SocketRequest *request = Encoders::ExtractSocketRequest(message->Payload);
        TRACE_MESSAGE("Socket: Creating socket on domain %d, type %d and protocol %d", request->Domain, request->Type, request->Protocol);

        response.Result = socket(request->Domain, request->Type, request->Protocol);
        if (response.Result < 0)
        {
            TRACE_MESSAGE("Socket: Error code: %d - %s", errno, strerror(errno));
            response.ResponseErrno = MapErrno(errno);
        }
        else
        {
            TRACE_MESSAGE("Socket: Created socket %d", response.Result);
            response.ResponseErrno = 0;
        }

        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Socket: Exit");
}

/**
 *  @brief Expose the POSIX connect method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the connect method.
 */
void WiFiRequestHandler::Connect(void *vpMessage)
{
    TRACE_MESSAGE("Connect: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::ConnectRequest *request = Encoders::ExtractConnectRequest(message->Payload);
        Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);
        if (request->AddrLength > 0)
        {
            if (request->Addr)
            {
                vPortFree(request->Addr);
            }
        }

        struct sockaddr_in sa = { };
        int family;
        Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
        errno = 0;
        if (family == AF_INET)
        {
            sa.sin_family = (uint8_t) (family & 0xff);
            sa.sin_port = socketAddress->Port;
            memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(sa.sin_addr));
            sa.sin_len = sizeof(struct sockaddr_in);
            TRACE_MESSAGE("Connect: Connecting socket: %d", request->SocketHandle);
            TRACE_MESSAGE("Connect: Family: %d", sa.sin_family);
            TRACE_MESSAGE("Connect: IP address: %s", inet_ntoa(sa.sin_addr));
            TRACE_MESSAGE("Connect: Port: %d", ntohs(sa.sin_port));
            response.Result = connect(request->SocketHandle, (struct sockaddr *) &sa, sizeof(sa));
        }
        else
        {
            response.Result = -1;
            errno = EFAULT;
        }

        if (response.Result != 0)
        {
            response.Result = -1;
            TRACE_MESSAGE("Connect: Error code: %d - %s", errno, strerror(errno));
            response.ResponseErrno = MapErrno(errno);
        }
        else
        {
            response.ResponseErrno = 0;
        }

        vPortFree(request);
        vPortFree(socketAddress);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Connect: Exit");
}

/**
 *  @brief Expose the POSIX freeaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the freeaddrinfo method.
 */
void WiFiRequestHandler::FreeAddrInfo(void  *vpMessage)
{
    TRACE_MESSAGE("FreeAddrInfo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    TRACE_MESSAGE("FreeAddrInfo: Releasing addrinfo storage.");
    Esp32Messaging::FreeAddrInfoRequest *request = Encoders::ExtractFreeAddrInfoRequest(message->Payload);

    freeaddrinfo(reinterpret_cast<struct addrinfo *>(request->AddrInfoAddress));

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = 0;
    message->Payload = nullptr;
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;

    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("FreeAddrInfo: Exit");
}

/**
 *  @brief Expose the POSIX getsockopt method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockopt method.
 */
void WiFiRequestHandler::GetSockOpt(void *vpMessage)
{
    TRACE_MESSAGE("GetSockOpt: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::GetSockOptRequest *request = Encoders::ExtractGetSockOptRequest(message->Payload);
    Esp32Messaging::GetSockOptResponse response = { };

    if (IsConnected())
    {
        int32_t errorCode = 0;
        struct timeval timeOut = { };
        struct linger lingerOut = { };
        int intVal = 0;
        int32_t optionName = 0;
        void *optionValue = nullptr;
        socklen_t optionLength = 0;

        if (Mapping::GetEspValue(Mapping::SocketOptions, request->OptionName, &optionName) < 0)
        {
            TRACE_MESSAGE("GetSockOpt: Unknown option: %d", request->OptionName);
            errorCode = ENOPROTOOPT;
        }
        else
        {
            errno = 0;
            switch (optionName)
            {
                case SO_SNDTIMEO:       /* Get the send timeout */
                case SO_RCVTIMEO:       /* Get the receive timeout */
                    optionValue = &timeOut;
                    optionLength = sizeof(struct timeval);
                    break;
                case SO_LINGER:         /* Linger on close if data present */
                    optionValue = &lingerOut;
                    optionLength = sizeof(struct linger);
                    break;
                case SO_RCVBUF:         /* Get the receive buffer size */
                    optionValue = &intVal;
                    optionLength = sizeof(intVal);
                    break;
                case SO_ACCEPTCONN:     /* Socket has had listen() */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_ACCEPTCONN");
                    break;
                case SO_DONTLINGER:     /* Don't linger on close if data present */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_DONTLINGER");
                    break;
                case SO_ERROR:          /* Get error status and clear */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_ERROR");
                    break;
                case SO_TYPE:           /* Get socket type */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_TYPE");
                    break;
                case SO_NO_CHECK:       /* Don't create UDP checksum */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_NO_CHECK");
                    break;
                default:
                    response.Result = -1;
                    errorCode = ENOPROTOOPT;
                    break;
            }
            if (errorCode == 0)
            {
                int32_t level = 0;
                Mapping::GetEspValue(Mapping::SocketLevel, request->Level, &level);
                TRACE_MESSAGE("GetSockOpt: Getting socket option level 0x%x, option name 0x%x for socket %d", level, optionName, request->SocketHandle);
                response.Result = getsockopt(request->SocketHandle, level, optionName, (void *) optionValue, &optionLength);
                errorCode = errno;
            }
        }
        if (errorCode == 0)
        {
            response.Result = 0;
            response.ResponseErrno = 0;
            response.OptionValue = NULL;
            Esp32Messaging::TimeVal esp_tv = { };
            Esp32Messaging::Linger esp_lv = { };
            Esp32Messaging::IntegerResponse esp_iv = { };
            switch (optionName)
            {
                case SO_SNDTIMEO:       /* Set the send timeout */
                case SO_RCVTIMEO:       /* Set the receive timeout */
                    esp_tv.TvSec = timeOut.tv_sec;
                    esp_tv.TvUsec = timeOut.tv_usec;
                    response.OptionValueLength = Encoders::EncodedTimeValBufferSize(&esp_tv);
                    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
                    if (response.OptionValue == NULL)
                    {
                        response.ResponseErrno = MapErrno(ENOMEM);
                        response.Result = -1;
                        response.OptionValueLength = 0;
                    }
                    else
                    {
                        Encoders::EncodeTimeVal(&esp_tv, response.OptionValue);
                    }
                    break;
                case SO_LINGER:         /* Linger on close if data present */
                    esp_lv.LLinger = lingerOut.l_linger;
                    esp_lv.LOnOff = lingerOut.l_onoff;
                    response.OptionValueLength = Encoders::EncodedLingerBufferSize(&esp_lv);
                    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
                    if (response.OptionValue == NULL)
                    {
                        response.ResponseErrno = MapErrno(ENOMEM);
                        response.Result = -1;
                        response.OptionValueLength = 0;
                    }
                    else
                    {
                        Encoders::EncodeLinger(&esp_lv, response.OptionValue);
                    }
                    break;
                case SO_RCVBUF:
                    esp_iv.Result = *((int *) optionValue);
                    response.OptionValueLength = Encoders::EncodedIntegerResponseBufferSize(&esp_iv);
                    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
                    if (response.OptionValue == NULL)
                    {
                        response.ResponseErrno = MapErrno(ENOMEM);
                        response.Result = -1;
                        response.OptionValueLength = 0;
                    }
                    else
                    {
                        Encoders::EncodeIntegerResponse(&esp_iv, response.OptionValue);
                    }
                    break;
            }
        }
        else
        {
            response.Result = -1;
            response.ResponseErrno = MapErrno(errorCode);
        }
        TRACE_MESSAGE("GetSockOpt: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));

        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    message->PayloadLength = Encoders::EncodedGetSockOptResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeGetSockOptResponse(&response, message->Payload);
    if (response.OptionValue != NULL)
    {
        free(response.OptionValue);
    }
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetSockOpt: Exit");
}

/**
 *  @brief Expose the POSIX setsockopt method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the setsockopt method.
 */
void WiFiRequestHandler::SetSockOpt(void *vpMessage)
{
    TRACE_MESSAGE("SetSockOpt: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::SetSockOptRequest *request = Encoders::ExtractSetSockOptRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        void *optionValue = nullptr;
        int optionLength = 0;
        struct timeval timeOut;
        int intVal = 0;
        int32_t errorCode = 0;

        int32_t optionName = 0;
        if (Mapping::GetEspValue(Mapping::SocketOptions, request->OptionName, &optionName) < 0)
        {
            TRACE_MESSAGE("SetSockOpt: Unknown option: 0x%x", request->OptionName);
            errorCode = ENOPROTOOPT;
        }
        else
        {
            errno = 0;
            switch (optionName)
            {
                case SO_SNDTIMEO:       /* Set the send timeout */
                case SO_RCVTIMEO:       /* Set the receive timeout */
                    {
                        Esp32Messaging::TimeVal *tv = Encoders::ExtractTimeVal(request->OptionValue);
                        timeOut.tv_sec = tv->TvSec;
                        timeOut.tv_usec = tv->TvUsec;
                        vPortFree(tv);
                        optionValue = &timeOut;
                        optionLength = sizeof(timeOut);
                    }
                    break;
                case SO_REUSEADDR:      /* Reuse local addresses when binding */
                case SO_RCVBUF:         /* Set the receive buffer size */
                    intVal = Encoders::ExtractInt32(request->OptionValue);
                    optionValue = &intVal;
                    optionLength = sizeof(intVal);
                    break;
                case SO_LINGER:         /* Linger on close if data present */
                    TRACE_MESSAGE("SetSockOpt: Implement SO_LINGER");
                    break;
                case SO_ACCEPTCONN:     /* Socket has had listen() */
                    TRACE_MESSAGE("SetSockOpt: Implement SO_ACCEPTCONN");
                    break;
                case SO_DONTLINGER:     /* Don't linger on close if data present */
                    TRACE_MESSAGE("SetSockOpt: Implement SO_DONTLINGER");
                    break;
                case SO_ERROR:          /* Get error status and clear */
                    TRACE_MESSAGE("SetSockOpt: Implement SO_ERROR");
                    break;
                case SO_TYPE:           /* Get socket type */
                    TRACE_MESSAGE("SetSockOpt: Implement SO_TYPE");
                    break;
                case SO_NO_CHECK:       /* Don't create UDP checksum */
                    TRACE_MESSAGE("SetSockOpt: Implement SO_NO_CHECK");
                    break;
                case SO_BROADCAST:
                    TRACE_MESSAGE("SetSockOpt: Implement SO_BROADCAST");
                    break;
                case SO_KEEPALIVE:
                    TRACE_MESSAGE("SetSockOpt: Implement SO_KEEPALIVE");
                    break;
                default:
                    response.Result = -1;
                    errorCode = ENOPROTOOPT;
                    break;
            }
            if (errorCode == 0)
            {
                int32_t level = 0;
                Mapping::GetEspValue(Mapping::SocketLevel, request->Level, &level);
                TRACE_MESSAGE("SetSockOpt: Setting socket option level to 0x%x, option name 0x%x for socket %d", level, optionName, request->SocketHandle);
                response.Result = setsockopt(request->SocketHandle, level, optionName, optionValue, optionLength);
                errorCode = errno;
            }
        }

        if (errorCode != 0)
        {
            response.Result = -1;
            response.ResponseErrno = MapErrno(errorCode);
        }
        TRACE_MESSAGE("SetSockOpt: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));

        if (request->OptionValueLength > 0)
        {
            if (request->OptionValue)
            {
                vPortFree(request->OptionValue);
            }
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("SetSockOpt: Exit");
}

/**
 *  @brief Expose the POSIX write method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the write method.
 */
void WiFiRequestHandler::Write(void *vpMessage)
{
    TRACE_MESSAGE("Write: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::WriteRequest *request = Encoders::ExtractWriteRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        response.Result = write(request->SocketHandle, request->Buffer, request->Count);
        if (response.Result < 0)
        {
            response.ResponseErrno = MapErrno(errno);;
        }

        if (request->BufferLength > 0)
        {
            if (request->Buffer)
            {
                vPortFree(request->Buffer);
            }
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Write: Exit");
}

/**
 *  @brief Expose the POSIX read method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the read method.
 */
void WiFiRequestHandler::Read(void *vpMessage)
{
    TRACE_MESSAGE("Read: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::ReadResponse response = { };
    response.Buffer = nullptr;

    if (IsConnected())
    {
        Esp32Messaging::ReadRequest *request = Encoders::ExtractReadRequest(message->Payload);
        response.Buffer = static_cast<uint8_t *>(pvPortMalloc(request->Count));
        bzero(response.Buffer, request->Count);
        response.BufferLength = request->Count;
        response.ReadResponseResult = read(request->SocketHandle, response.Buffer, request->Count);
        if (response.ReadResponseResult < 0)
        {
            response.ReadResponseErrno = MapErrno(errno);
        }
        vPortFree(request);
    }
    else
    {
        response.ReadResponseResult = -1;
        response.ReadResponseErrno = MapErrno(ENETDOWN)        ;
    }

    vPortFree(message->Payload);

    //
    //  TODO: Optimise this by not sending an empty buffer in the case of an error.
    //
    message->PayloadLength = Encoders::EncodedReadResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeReadResponse(&response, message->Payload);

    if (response.Buffer)
    {
        vPortFree(response.Buffer);
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Read: Exit");
}

/**
 *  @brief Expose the POSIX close method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the close method.
 */
void WiFiRequestHandler::Close(void *vpMessage)
{
    TRACE_MESSAGE("Close: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::CloseRequest *request = Encoders::ExtractCloseRequest(message->Payload);

        response.Result = close(request->SocketHandle);
        if (response.Result != 0)
        {
            if (response.Result < 0)
            {
                response.Result = MapErrno(errno);
            }
        }

        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Close: Exit");
}

/**
 *  @brief Expose the POSIX send method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the send method.
 */
void WiFiRequestHandler::Send(void *vpMessage)
{
    TRACE_MESSAGE("Send: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::SendRequest *request = Encoders::ExtractSendRequest(message->Payload);
        int32_t mappedFlags = 0;
        response.ResponseErrno = 0;
        response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
        if (response.Result == 0)
        {
            response.Result = send(request->SocketHandle, request->Buffer, request->Length, mappedFlags);
            if (response.Result < 0)
            {
                response.ResponseErrno = MapErrno(errno);
            }
        }
        else
        {
            response.ResponseErrno = MapErrno(EFAULT);
        }

        TRACE_MESSAGE("Send: Sent %d bytes.", request->Length);
        if (request->Buffer && (request->Length > 0))
        {
            TRACE_HEX_BUFFER(request->Buffer, request->Length);
        }
        TRACE_MESSAGE("Send: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));

        vPortFree(request->Buffer);
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Send: Exit");
}

/**
 *  @brief Expose the POSIX sendto method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the sendto method.
 */
void WiFiRequestHandler::SendTo(void *vpMessage)
{
    TRACE_MESSAGE("SendTo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::SendToRequest *request = Encoders::ExtractSendToRequest(message->Payload);

        int32_t mappedFlags = 0;
        response.ResponseErrno = 0;
        response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
        if (response.Result == 0)
        {
            Esp32Messaging::SockAddr *socketAddress = NULL;
            int32_t family = 0;
            if (request->DestinationAddress)
            {
                socketAddress = Encoders::ExtractSockAddr(request->DestinationAddress);
                Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
                TRACE_MESSAGE("SendTo: Mapping request family %d to ESP family %d", socketAddress->Family, family);
            }
            
            struct sockaddr_in sa = { };
            struct sockaddr *psa = NULL;
            size_t lenSa = 0;
            if (request->DestinationAddressLength != 0)
            {
                if (family == AF_INET)
                {
                    if (request->DestinationAddress)
                    {
                        sa.sin_family = (unsigned char) (family & 0xff);

                        sa.sin_port = socketAddress->Port;
                        memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(sa.sin_addr));
                        sa.sin_len = sizeof(sa);
                        psa = (struct sockaddr *) &sa;
                        lenSa = sizeof(sa);
                    }
                    TRACE_MESSAGE("SendTo: Connecting socket: %d", request->SocketHandle);
                    TRACE_MESSAGE("SendTo: Family: %d", (psa == NULL) ? 0 : sa.sin_family);
                    TRACE_MESSAGE("SendTo: IP address: %s", (psa == NULL) ? "NULL" : inet_ntoa(sa.sin_addr));
                    TRACE_MESSAGE("SendTo: Port: %d", (psa == NULL) ? 0 : ntohs(sa.sin_port));
                }
            }
            response.Result = sendto(request->SocketHandle, request->Buffer, request->Length, mappedFlags, psa, lenSa);

            if (socketAddress != NULL)
            {
                vPortFree(socketAddress);
            }

            if (response.Result < 0)
            {
                response.ResponseErrno = MapErrno(errno);
            }
        }
        else
        {
            response.ResponseErrno = MapErrno(EFAULT);
        }

        TRACE_MESSAGE("SendTo: Sent %d bytes.", response.Result);
        if (request->Buffer && (request->Length > 0))
        {
            TRACE_HEX_BUFFER(request->Buffer, request->Length);
        }
        TRACE_MESSAGE("SendTo: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));

        if (request->DestinationAddress)
        {
            vPortFree(request->DestinationAddress);
        }
        vPortFree(request->Buffer);
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("SendTo: Exit");
}

/**
 *  @brief Expose the POSIX recvfrom method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the recvfrom method.
 */
void WiFiRequestHandler::RecvFrom(void *vpMessage)
{
    TRACE_MESSAGE("RecvFrom: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::RecvFromResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::RecvFromRequest *request = Encoders::ExtractRecvFromRequest(message->Payload);
        int32_t mappedFlags = 0;
        response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
        if (response.Result == 0)
        {
            struct sockaddr_in *sa = NULL;
            socklen_t len;
            if (request->GetSourceAddress != 0)
            {
                sa = (struct sockaddr_in *) pvPortMalloc(sizeof(struct sockaddr_in));
                bzero(sa, sizeof(struct sockaddr_in));
                len = sizeof(struct sockaddr_in);
            }
            response.Buffer = (uint8_t *) pvPortMalloc(request->Length);
            if (response.Buffer == NULL)
            {
                response.Buffer = nullptr;
                response.Result = -1;
                errno = ENOMEM;
            }
            else
            {
                response.Result = recvfrom(request->SocketHandle, response.Buffer, request->Length, mappedFlags, (struct sockaddr *) sa, (sa == NULL) ? NULL : &len);
            }

            if (response.Result < 0)
            {
                response.ResponseErrno = errno;
            }
            else
            {
                if (request->GetSourceAddress != 0)
                {
                    Esp32Messaging::SockAddr socketAddress = { };
                    int family;
                    Mapping::GetStmValue(Mapping::AddressFamily, sa->sin_family, &family);
                    socketAddress.Family = (uint8_t) (family & 0xff);
                    memcpy(&socketAddress.Ip4Address, &sa->sin_addr, sizeof(sa->sin_addr));
                    socketAddress.Port = sa->sin_port;
                    response.SourceAddressLength = Encoders::EncodedSockAddrBufferSize(&socketAddress);
                    response.SourceAddress = (uint8_t *) pvPortMalloc(response.SourceAddressLength);
                    if (response.SourceAddress == NULL)
                    {
                        response.SourceAddressLength = 0;
                        response.SourceAddressLen = 0;
                        response.Result = -1;
                    }
                    else
                    {
                        Encoders::EncodeSockAddr(&socketAddress, response.SourceAddress);
                    }
                }
                else
                {
                    response.SourceAddress = nullptr;
                    response.SourceAddressLength = 0;
                    response.SourceAddressLen = 0;
                }
                response.BufferLength = response.Result;
            }

            if (sa != NULL)
            {
                vPortFree(sa);
            }
        }
        else
        {
            response.ResponseErrno = EFAULT;
        }
        TRACE_MESSAGE("RecvFrom: Received %d bytes.", response.Result);
        if (response.Buffer && (response.BufferLength > 0))
        {
            TRACE_HEX_BUFFER(response.Buffer, response.Result);
        }
        TRACE_MESSAGE("RecvFrom: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));
        if (response.ResponseErrno != 0)
        {
            response.ResponseErrno = MapErrno(response.ResponseErrno);
        }

        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedRecvFromResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeRecvFromResponse(&response, message->Payload);
    if (response.Buffer)
    {
        vPortFree(response.Buffer);
    }
    if (response.SourceAddress)
    {
        vPortFree(response.SourceAddress);
    }
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("RecvFrom: Exit");
}

/**
 *  @brief Expose the POSIX bind method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the bind method.
 */
void WiFiRequestHandler::Bind(void *vpMessage)
{
    TRACE_MESSAGE("Bind: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response =  { };

    if (IsConnected())
    {
        Esp32Messaging::BindRequest *request = Encoders::ExtractBindRequest(message->Payload);
        Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);
        if (request->AddrLength > 0)
        {
            if (request->Addr)
            {
                vPortFree(request->Addr);
                request->Addr = nullptr;
            }
        }

        struct sockaddr_in sa = { };
        int family;
        Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
        errno = 0;
        if (family == AF_INET)
        {
            sa.sin_family = socketAddress->Family;
            sa.sin_port = socketAddress->Port;
            memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(socketAddress->Ip4Address));
            TRACE_MESSAGE("Bind: Socket handle: %d", request->SocketHandle);
            TRACE_MESSAGE("Bind: Family: %d", sa.sin_family);
            TRACE_MESSAGE("Bind: IP address: %s", inet_ntoa(sa.sin_addr));
            TRACE_MESSAGE("Bind: Port: %d", ntohs(sa.sin_port));
            response.Result = bind(request->SocketHandle, (struct sockaddr *) &sa, sizeof(sa));
        }
        else
        {
            response.Result = -1;
            errno = EFAULT;
        }

        if (response.Result != 0)
        {
            response.ResponseErrno = MapErrno(errno);
            response.Result = -1;
        }
        else
        {
            response.ResponseErrno = 0;
        }
        TRACE_MESSAGE("Bind: Error code: %d - %s", errno, strerror(errno));
        vPortFree(request);
        vPortFree(socketAddress);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Bind: Exit");
}

/**
 *  @brief Expose the POSIX listen method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the listen method.
 */
void WiFiRequestHandler::Listen(void *vpMessage)
{
    TRACE_MESSAGE("Listen: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::ListenRequest *request = Encoders::ExtractListenRequest(message->Payload);
        response.Result = listen(request->SocketHandle, request->BackLog);
        response.ResponseErrno = (response.Result == 0) ? 0 : MapErrno(errno);
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    PERFORMANCE_LOGGING_DUMP_RESULTS();
    TRACE_MESSAGE("Listen: Exit");
}

/**
 *  @brief Expose the POSIX accept method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the accept method.
 */
void WiFiRequestHandler::Accept(void *vpMessage)
{
    TRACE_MESSAGE("Accept: Enter");
    PERFORMANCE_LOGGING_ENTER();
    Esp32Messaging::AcceptResponse response = { };
    Message *message = static_cast<Message *>(vpMessage);

    if (IsConnected())
    {
        Esp32Messaging::AcceptRequest *request = Encoders::ExtractAcceptRequest(message->Payload);

        struct sockaddr sa = { };
        socklen_t saLen = sizeof(sa);
        response.Result = accept(request->SocketHandle, &sa, &saLen);
        if (response.Result > 0)
        {
            Esp32Messaging::SockAddr *sockAddr = static_cast<Esp32Messaging::SockAddr *>(pvPortMalloc(sizeof(Esp32Messaging::SockAddr)));
            bzero(sockAddr, sizeof(Esp32Messaging::SockAddr));
            int family;
            Mapping::GetStmValue(Mapping::AddressFamily, sa.sa_family, &family);
            sockAddr->Family = (uint8_t) (family & 0xff);
            struct sockaddr_in *sin = (struct sockaddr_in *) &sa;
            sockAddr->Port = sin->sin_port;
            memcpy(&sockAddr->Ip4Address, &sin->sin_addr, sizeof(sin->sin_addr));
            response.AddrLength = Encoders::EncodedSockAddrBufferSize(sockAddr);
            response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
            Encoders::EncodeSockAddr(sockAddr, response.Addr);
            vPortFree(sockAddr);
            response.ResponseErrno = 0;
            TRACE_MESSAGE("Accept: Socket handle: %d", response.Result);
            TRACE_MESSAGE("Accept: Family: %d", sin->sin_family);
            TRACE_MESSAGE("Accept: IP address: %s", inet_ntoa(sin->sin_addr));
            TRACE_MESSAGE("Accept: Port: %d", ntohs(sin->sin_port));
        }
        else
        {
            response.AddrLength = 0;
            response.Addr = nullptr;
            response.ResponseErrno = MapErrno(errno);
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedAcceptResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeAcceptResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Accept: Exit");
}

/**
 *  @brief Expose the simulation of the POSIX ioctl method to the STM32.
 *
 *  This is a simulation of the ioctl method as this is not supported by the
 *  ESP-IDF framework.
 *
 *  @param message
 *      Message structure containing the parameters for the ioctl method.
 */
void WiFiRequestHandler::Ioctl(void *vpMessage)
{
    TRACE_MESSAGE("Ioctl: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IoctlResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::IoctlRequest *request = Encoders::ExtractIoctlRequest(message->Payload);

        esp_netif_ip_info_t ipInfo;
        Esp32Messaging::SockAddr sa = { };
        int family;

        Mapping::GetStmValue(Mapping::AddressFamily, AF_INET, &family);
        sa.Family = (unsigned char) (family & 0xff);
        response.AddrLength = 0;
        response.Addr = nullptr;

        switch (request->Command)
        {
            case SIOCGIFNETMASK:    /* Get network mask */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    memcpy(&sa.Ip4Address, &ipInfo.netmask, sizeof(ipInfo.netmask));
                    response.AddrLength = Encoders::EncodedSockAddrBufferSize(&sa);
                    response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
                    Encoders::EncodeSockAddr(&sa, response.Addr);
                }
                break;
            case SIOCGIFHWADDR:     /* Get hardware address */
                response.AddrLength = 6;
                response.Addr = static_cast<uint8_t *>(pvPortMalloc(6));
                GetMacAddress(response.Addr, WIFI_IF_STA);
                break;
            case SIOCGIFADDR:       /* Get IP address */
            case SIOCGIFCONF:       /* Return an interface list (IPv4) */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    memcpy(&sa.Ip4Address, &ipInfo.ip, sizeof(ipInfo.ip));
                    response.AddrLength = Encoders::EncodedSockAddrBufferSize(&sa);
                    response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
                    Encoders::EncodeSockAddr(&sa, response.Addr);
                }
                break;
            case SIOCGIFFLAGS:      /* Gets the interface flags */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    response.Flags = IFF_UP;
                }
                else
                {
                    response.Flags = IFF_DOWN;
                }
                break;
            default:
                response.Result = -1;
                response.ResponseErrno = MapErrno(EINVAL);
                TRACE_MESSAGE("Unknown ioctl request %04x", request->Command);
                break;
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIoctlResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIoctlResponse(&response, message->Payload);
    if (response.Addr)
    {
        vPortFree(response.Addr);
    }
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Ioctl: Exit");
}

/**
 *  @brief Expose the POSIX getsockname method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockname method.
 *
 *  @param methodToCall
 *      Which method should be called, getsockname or getpeername?
 */
void WiFiRequestHandler::GetSockPeerName(void *vpMessage, WiFiFunction::WiFiFunction methodToCall)
{
    TRACE_MESSAGE("GetSockPeerName: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::GetSockPeerNameResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::GetSockPeerNameRequest *request = Encoders::ExtractGetSockPeerNameRequest(message->Payload);

        struct sockaddr sa = { };
        socklen_t saLen = sizeof(sa);
        if (methodToCall == WiFiFunction::GetSockName)
        {
            response.Result = getsockname(request->SocketHandle, &sa, &saLen);
        }
        else
        {
            response.Result = getpeername(request->SocketHandle, &sa, &saLen);
        }
        if (response.Result == 0)
        {
            Esp32Messaging::SockAddr *sockAddr = static_cast<Esp32Messaging::SockAddr *>(pvPortMalloc(sizeof(Esp32Messaging::SockAddr)));
            bzero(sockAddr, sizeof(Esp32Messaging::SockAddr));
            int family;
            Mapping::GetStmValue(Mapping::AddressFamily, sa.sa_family, &family);
            sockAddr->Family = (uint8_t) (family & 0xff);
            struct sockaddr_in *sin = (struct sockaddr_in *) &sa;
            sockAddr->Port = sin->sin_port;
            memcpy(&sockAddr->Ip4Address, &sin->sin_addr, sizeof(sin->sin_addr));
            response.AddrLength = Encoders::EncodedSockAddrBufferSize(sockAddr);
            response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
            Encoders::EncodeSockAddr(sockAddr, response.Addr);
            vPortFree(sockAddr);
            response.ResponseErrno = 0;
            TRACE_MESSAGE("GetSockPeerName: Socket handle: %d", response.Result);
            TRACE_MESSAGE("GetSockPeerName: Family: %d", sin->sin_family);
            TRACE_MESSAGE("GetSockPeerName: IP address: %s", inet_ntoa(sin->sin_addr));
            TRACE_MESSAGE("GetSockPeerName: Port: %d", ntohs(sin->sin_port));
        }
        else
        {
            response.AddrLength = 0;
            response.Addr = nullptr;
            response.ResponseErrno = MapErrno(errno);
            ERROR_MESSAGE("Failure: Result code %d (%s), errno %d (%s)", response.Result, esp_err_to_name(response.Result), errno, strerror(errno));
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedGetSockPeerNameResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeGetSockPeerNameResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetSockPeerName: Exit");
}

/**
 *  @brief Expose the POSIX getsockname method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockname method.
 */
void WiFiRequestHandler::GetSockName(void *vpMessage)
{
    TRACE_MESSAGE("GetSockName: Enter");
    GetSockPeerName(vpMessage, WiFiFunction::GetSockName);
    TRACE_MESSAGE("GetSockName: Exit");
}

/**
 *  @brief Expose the POSIX getpeername method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getpeername method.
 */
void WiFiRequestHandler::GetPeerName(void *vpMessage)
{
    TRACE_MESSAGE("GetPeerName: Enter");
    GetSockPeerName(vpMessage, WiFiFunction::GetPeerName);
    TRACE_MESSAGE("GetPeerName: Exit");
}

/**
 *  @brief Lock the _pollRequests object.
 */
void WiFiRequestHandler::LockPollRequests()
{
     xSemaphoreTake(_pollRequestListMutex, portMAX_DELAY);
}

/**
 *  @brief Unlock the _pollRequests object.
 */
void WiFiRequestHandler::UnlockPollRequests()
{
     xSemaphoreGive(_pollRequestListMutex);
}

/**
 *  @brief Open a dummy socket to be used by the Poll methods to terminate a poll request.
 * 
 *  @returns
 *      Handle to the socket that has just been opened of -1 if there is a problem.
 */
int WiFiRequestHandler::OpenDummySocket()
{
    TRACE_MESSAGE("OpenDummySocket: Enter");

    struct addrinfo hints = { };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *res = NULL;

    int dummy_socket_fd = -1;
    if (getaddrinfo("localhost", "80", &hints, &res) >= 0)
    {
        dummy_socket_fd = socket(res->ai_family, res->ai_socktype, 0);
    }
    TRACE_MESSAGE("OpenDummySocket: Exit, result %d", dummy_socket_fd);

    return(dummy_socket_fd);
}

/**
 *  @brief Debug method to dump all of the active poll requests.
 */
void WiFiRequestHandler::DumpActivePollRequests()
{
    TRACE_MESSAGE("DumpActivePollRequests: Enter");
    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.begin();
    while (it != _pollRequests.end())
    {
        struct pollfd *fds = static_cast<struct pollfd *>(it->second);
        TRACE_MESSAGE("DumpActiveRequests: Key %08x, socket %d, dummy socket %d", it->first, fds[POLL_FD_INDEX].fd, fds[POLL_DUMMY_FD_INDEX].fd);
        it++;
    }
    UnlockPollRequests();
    TRACE_MESSAGE("DumpActivePollRequests: Exit");
}

/**
 *  @brief Setup a poll request an assign it to a thread in the thread pool.
 * 
 *  @param sender
 *      Instance of the class that is making the request.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
int WiFiRequestHandler::PollSetup(void *vpMessage)
{
    TRACE_MESSAGE("PollSetup: Enter");

    int result = 0;
    if (vpMessage == NULL)
    {
        return(-1);
    }
    struct pollfd *fds = static_cast<struct pollfd *>(pvPortMalloc(2 * sizeof(struct pollfd)));
    if (fds == NULL)
    {
        return(-1);
    }

    int dummy_socket_handle = OpenDummySocket();
    if (dummy_socket_handle < 0)
    {
        vPortFree(fds);
        return(-1);
    }

    Esp32Messaging::PollRequest *request = static_cast<Esp32Messaging::PollRequest *>(vpMessage);
    memset(fds, 0, 2 * sizeof(struct pollfd));
    int32_t events;
    Mapping::GetEspFlagValue(Mapping::PollEvents, request->Events, &events);

    fds[WiFiRequestHandler::POLL_FD_INDEX].fd = request->SocketHandle;
    fds[WiFiRequestHandler::POLL_FD_INDEX].events = (uint16_t) (events & 0xffff);
    fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd = dummy_socket_handle;
    fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].events = POLLIN;

    TRACE_MESSAGE("Poll: Requested Nuttx events 0x%04x maps to ESP events 0x%04x", request->Events, events);

    LockPollRequests();
    _pollRequests[request->SetupMessageId] = fds;
    UnlockPollRequests();

    #if defined(WIFI_DEBUG)
        DumpActivePollRequests();
    #endif

    SystemRequestHandler::SharedThreadPool()->Execute(PollSocket, (void *) request->SetupMessageId);

    TRACE_MESSAGE("PollSetup: Exit, result %d", result);

    return(result);
}

/**
 *  @brief Teardown a poll request that has previously been setup by PollSetup.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
int WiFiRequestHandler::PollTeardown(void *vpMessage)
{
    TRACE_MESSAGE("PollTeardown: Enter");
    int result = 0;

    Esp32Messaging::PollRequest *request = (Esp32Messaging::PollRequest *) vpMessage;

    #if defined(WIFI_DEBUG)
        DumpActivePollRequests();
    #endif

    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.find(request->SetupMessageId);
    if (it != _pollRequests.end())
    {
        TRACE_MESSAGE("PollTeardown: Found socket handle for request %08x", request->SetupMessageId);
        struct pollfd *fds = static_cast<struct pollfd *>(it->second);
        result = write(fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd, "E", 1);      // Writing to the dummy file descriptor should end the poll request.
        if (close(fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd) < 0)
        {
            result = -1;
        }
        vPortFree(fds);
        _pollRequests.erase(it);
    }
    else
    {
        //
        //  We do not treat this as an error as the request information may have been removed by the 
        //  interrupt handler.
        //
        TRACE_MESSAGE("PollTeardown: Cannot find request %08x", request->SetupMessageId);
    }
    UnlockPollRequests();

    #if defined(WIFI_DEBUG)
        DumpActivePollRequests();
    #endif

    TRACE_MESSAGE("PollTeardown: Exit, result %d", result);

    return(result);
}

/**
 *  @brief Execute the actual request to poll a socket.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
void WiFiRequestHandler::PollSocket(void *requestId)
{
    TRACE_MESSAGE("PollSocket: Enter");

    Esp32Messaging::InterruptPollResponse response = { };

    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.find((uint32_t) requestId);
    UnlockPollRequests();
    if (it != _pollRequests.end())
    {
        struct pollfd *fds = (struct pollfd *) it->second;
        response.Result = poll(fds, 2, -1);
        if (response.Result < 0)
        {
            response.ResponseErrno = MapErrno(errno);
        }
        else
        {
            LockPollRequests();
            it = _pollRequests.find((uint32_t) requestId);      // Get again in cse anything has changed.
            if (it != _pollRequests.end())
            {
                int32_t events;
                Mapping::GetStmFlagValue(Mapping::PollEvents, fds[POLL_FD_INDEX].revents, &events);
                response.ReturnedEvents = (uint16_t) (events & 0xffff);
                TRACE_MESSAGE("Poll: Returned ESP32 events 0x%04x maps to NuttX events 0x%04x", fds[POLL_FD_INDEX].revents, events);
                response.SetupMessageId = (uint32_t) requestId;
                close(fds[POLL_DUMMY_FD_INDEX].fd);
                vPortFree(fds);
                _pollRequests.erase(it);
            }
            else
            {
                response.Result = -1;
                response.ResponseErrno = MapErrno(EFAULT);
            }
            UnlockPollRequests();
        }
    }
    TRACE_MESSAGE("Poll: Returned value: %d", response.Result);

    Message *message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));
    message->Interface = Esp32Interfaces::WiFi;
    message->Function = WiFiFunction::InterruptPollResponse;
    message->MessageID = _messageDispatcher->GetNextMessageID();
    message->PayloadLength = Encoders::EncodedInterruptPollResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));

    Encoders::EncodeInterruptPollResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Event;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("PollSocket: Exit");
}

/**
 *  @brief Expose the poll method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
void WiFiRequestHandler::Poll(void *vpMessage)
{
    TRACE_MESSAGE("Poll: Enter");
    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::PollRequest *request = Encoders::ExtractPollRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (request->Setup == 1)
    {
        response.Result = PollSetup((void *) request);
    }
    else
    {
        response.Result = PollTeardown((void *) request);
    }

    if (response.Result < 0)
    {
        response.ResponseErrno = MapErrno(errno);
    }
    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("Poll: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Event Handlers
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Process the station started event.
 */
void WiFiRequestHandler::StationStartedEvent()
{
    TRACE_MESSAGE("StationStartedEvent: Enter");
    _lastIdfErrorCode = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, SystemRequestHandler::GetDeviceName());
    if (_lastIdfErrorCode != ESP_OK)
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_INTERFACE_STARTING_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
        RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::StartWiFiInterfaceEvent, StatusCodes::Failure);
    }
    else
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT);
        RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::StartWiFiInterfaceEvent);
    }
    TRACE_MESSAGE("StationStartedEvent: Exit");
}

/**
 *  @brief Process the station stopped event.
 */
void WiFiRequestHandler::StationStoppedEvent()
{
    TRACE_MESSAGE("StationStoppedEvent: Enter");
 
    _stationNetIfHandle = nullptr;
    RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::StopWiFiInterfaceEvent);

    TRACE_MESSAGE("StationStoppedEvent: Exit");
}

/**
 *  @brief Process the access point started event.
 */
void WiFiRequestHandler::AccessPointStartedEvent()
{
    TRACE_MESSAGE("AccessPointStartedEvent: Enter");

    TRACE_MESSAGE("AccessPointStartedEvent: Exit");
}

/**
 *  @brief Process the access point stopped event.
 */
void WiFiRequestHandler::AccessPointStoppedEvent()
{
    TRACE_MESSAGE("AccessPointStoppedEvent: Enter");

    _softApNetIfHandle = nullptr;

    TRACE_MESSAGE("AccessPointStoppedEvent: Exit");
}

/**
 *  @brief The access point has just given the ESP32 an IP address.
 *
 *  This event indicates that we have successfully obtained an IP address.  It is at this
 *  point that we really consider the device to be connected to the access point as we can
 *  start to perform network access operations.
 *
 *  @param eventData
 *      Data for the IP event.
 */
void WiFiRequestHandler::StationGotIpEvent(ip_event_got_ip_t *eventData)
{
    TRACE_MESSAGE("StationGotIpEvent: Enter");
    _currentRetryCount = 0;
    memcpy((void *) &_ipInformation, (void *) &eventData->ip_info, sizeof(esp_netif_ip_info_t));
    _ipChanged = eventData->ip_changed;

    if (GetGetTimeAtStartup())
    {
        //
        //  Note that we only need to get the time here.  The timezone is set when this
        //  class is instanciated or when the SetTimezone method is successfully called.
        //
        TRACE_MESSAGE("Getting time from %s", GetNtpServer());
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, GetNtpServer());
        sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
        sntp_set_time_sync_notification_cb(NtpServerCallback);
        sntp_init();
    }

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);
    TRACE_MESSAGE("StationGotIpEvent: Exit");
}

/**
 *  @brief Process the WiFi station connect event.
 *
 *  This event will fire when the ESP has successfully connected to an access point.
 *
 *  @param eventData
 *      Data for the connection event.
 */
void WiFiRequestHandler::StationConnectedEvent(wifi_event_sta_connected_t *eventData)
{
    TRACE_MESSAGE("StationConnectedEvent: Enter");
    memcpy((void *) &_apInformation, (void *) eventData, sizeof(wifi_event_sta_connected_t));

    EventBits_t bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_RECONNECTING_BIT , pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);

    if (bits & WIFI_RECONNECTING_BIT)
    {
        RaiseConnectToAccessPointEvent(StatusCodes::CompletedOk);
    }

    TRACE_MESSAGE("StationConnectedEvent: Exit");
}

/**
 *  @brief Process the WiFi disconnected event.
 *
 *  @param eventData
 *      Data for the disconnected event.
 */
void WiFiRequestHandler::StationDisconnectedEvent(wifi_event_sta_disconnected_t *eventData)
{
    TRACE_MESSAGE("StationDisconnectedEvent: Enter");

    EventBits_t bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_RECONNECTING_BIT | WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT, pdFALSE, pdFALSE, 0);
    if (bits & WIFI_DISCONNECT_REQUESTED_BIT)
    {
        return;
    }

    uint32_t maximumRetryCount = GetMaximumRetryCount();
    if (bits & (WIFI_CONNECTED_BIT | WIFI_RECONNECTING_BIT | WIFI_CONNECTING_BIT))
    {
        bool raiseEvent = true;
        if (bits & (WIFI_CONNECTED_BIT | WIFI_RECONNECTING_BIT))
        {
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);
            if (_automaticReconnect && (_currentRetryCount < maximumRetryCount))
            {
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_RECONNECTING_BIT);
                esp_wifi_connect();
                _currentRetryCount++;
                raiseEvent = false;
            }
        }
        else
        {
            if (_currentRetryCount < maximumRetryCount)
            {
                esp_wifi_connect();
                _currentRetryCount++;
                raiseEvent = false;
            }
        }
        if (raiseEvent)
        {
            ClearIpAndDisconnectData();
            bzero((void *) &_apInformation, sizeof(wifi_event_sta_connected_t));
            RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::DisconnectFromAccessPointEvent);
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_RECONNECTING_BIT | WIFI_CONNECTING_BIT);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
        }
    }

    TRACE_MESSAGE("StationDisconnectedEvent: Exit");
}
