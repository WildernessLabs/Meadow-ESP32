/*
 *  WiFiRequestHandler.cpp
 *
 *  Implementation of the WiFi functionality.
 */
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
#include "DebugSettings.hpp"

#if defined(CONFIG_HEAP_TRACING_STANDALONE) || defined(CONFIG_HEAP_TRACING_TOHOST)

#include "esp_heap_trace.h"

#endif

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

#define NUM_RECORDS 100
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
bool WiFiRequestHandler::_instantiated = false;

/**
 *  @brief List of active poll requests.
 */
GenericLinkList<WiFiRequestHandler::PollRequestInformation> *WiFiRequestHandler::_pollRequests = nullptr;

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
    if (_instantiated)
    {
        throw new MultipleInstancesException();
    }
    else
    {
        _instantiated = true;
    }
    //
    //  Get configuration from NVS.
    //
    _maximumRetryCount = SystemRequestHandler::GetConfigurationValue(MAXIMUM_RETRY_COUNT_NAME, _maximumRetryCount);
    _automaticReconnect = SystemRequestHandler::GetConfigurationValue(AUTOMATIC_RECONNECT_NAME, _automaticReconnect);
    _automaticallyStartNetwork = SystemRequestHandler::GetConfigurationValue(AUTOMATICALLY_START_NETWORK_NAME, _automaticallyStartNetwork);
    _getTimeAtStartup = SystemRequestHandler::GetConfigurationValue(GET_TIME_AT_STARTUP_NAME, _getTimeAtStartup);
    _useDhcp = SystemRequestHandler::GetConfigurationValue(USE_DHCP_NAME, _useDhcp);
    _defaultAccessPoint = SystemRequestHandler::GetConfigurationValue(DEFAULT_ACCESS_POINT_NAME, _defaultAccessPoint);
    _password = SystemRequestHandler::GetConfigurationValue(PASSWORD_NAME, _password);
    _ntpServer = SystemRequestHandler::GetConfigurationValue(NTP_SERVER_NAME, _ntpServer);
    _timezone = SystemRequestHandler::GetConfigurationValue(TIMEZONE_NAME, _timezone);
    _staticIpAddress = SystemRequestHandler::GetConfigurationValue(STATIC_IP_ADDRESS_NAME, _staticIpAddress);
    _dnsServer = SystemRequestHandler::GetConfigurationValue(DNS_SERVER_NAME, _dnsServer);
    _defaultGateway = SystemRequestHandler::GetConfigurationValue(DEFAULT_GATEWAY_NAME, _defaultGateway);
    _wifiTimeout = SystemRequestHandler::GetConfigurationValue(DEFAULT_WIFI_TIMEOUT_NAME, _wifiTimeout);
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
    _maximumRetryCount = SystemRequestHandler::GetConfigurationValue(MAXIMUM_RETRY_COUNT_NAME, _maximumRetryCount);
    _pollRequestListMutex = xSemaphoreCreateMutex();
    if (_pollRequestListMutex == NULL)
    {
        ESP_LOGE(COMPONENT_NAME, "WiFiRequestHandler: Cannot create poll request list mutex.");
    }
    _pollRequests = new GenericLinkList<PollRequestInformation>(ComparePollRequestMessageIds);
    _completedPollRequestsQueue = xQueueCreate(MAX_QUEUE_LENGTH, sizeof(PollRequestInformation *));
    if (_completedPollRequestsQueue == NULL)
    {
        ESP_LOGE(COMPONENT_NAME, "WiFiRequestHandler: Cannot allocate completed poll requests queue.");
    }
    xTaskCreate(CompletedPollRequestProcessor, "CompletedPollRequests", 2048 * 2, NULL, configMAX_PRIORITIES - 1, &_completedPollRequestsTaskHandle);
    //
    //  All of the requests should be handled through the ThreadPool to allow for multiple
    //  requests (upto 10 in this case) to be processed simultaneously.
    //
    _threadPool = new ThreadPool(10);       // cppcheck-suppress [ noOperatorEq, noCopyConstructor ]
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
    delete _threadPool;
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
    return(_maximumRetryCount);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _maximumRetryCount)
    {
        if (SystemRequestHandler::SetConfigurationValue(MAXIMUM_RETRY_COUNT_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _maximumRetryCount = value;
        }
        else
        {
            result = StatusCodes::Failure;
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _automaticReconnect)
    {
        if (SystemRequestHandler::SetConfigurationValue(AUTOMATIC_RECONNECT_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _automaticReconnect = value;
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _automaticallyStartNetwork)
    {
        if (SystemRequestHandler::SetConfigurationValue(AUTOMATICALLY_START_NETWORK_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _automaticallyStartNetwork = value;
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _getTimeAtStartup)
    {
        if (SystemRequestHandler::SetConfigurationValue(GET_TIME_AT_STARTUP_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _getTimeAtStartup = value;
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _useDhcp)
    {
        if (SystemRequestHandler::SetConfigurationValue(USE_DHCP_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _useDhcp = value;
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (strcmp(value, _ntpServer) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(NTP_SERVER_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _ntpServer = strdup(value);
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (strcmp(value, _timezone) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(TIMEZONE_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _timezone = strdup(value);
            setenv("TZ", value, 1);
            tzset();
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (strcmp(value, _defaultAccessPoint) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(DEFAULT_ACCESS_POINT_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (strcmp(value, _password) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(PASSWORD_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _staticIpAddress)
    {
        if (SystemRequestHandler::SetConfigurationValue(STATIC_IP_ADDRESS_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _staticIpAddress = value;
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _dnsServer)
    {
        if (SystemRequestHandler::SetConfigurationValue(DNS_SERVER_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _dnsServer = value;
        }
    }
    return(result);
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _defaultGateway)
    {
        if (SystemRequestHandler::SetConfigurationValue(DEFAULT_GATEWAY_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _defaultGateway = value;
        }
    }
    return(result);
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
    if (esp_wifi_get_mac(interface, macAddress) != ESP_OK)
    {
        for (int index = 0; index < 6; index++)
        {
            *macAddress = 0;
            macAddress++;
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    return(result);
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
        SystemRequestHandler::SetConfigurationValue(ANTENNA_TYPE_NAME, antenna);
        SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(antenna));
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
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (value != _wifiTimeout)
    {
        if (SystemRequestHandler::SetConfigurationValue(DEFAULT_WIFI_TIMEOUT_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _wifiTimeout = value;
        }
        else
        {
            result = StatusCodes::Failure;
        }
    }
    return(result);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

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
    ESP_LOGI(COMPONENT_NAME, "ConvertAntennaToPin: Enter");
    ESP_LOGI(COMPONENT_NAME, "ConvertAntennaToPin: Exit");

    return((antenna == AntennaTypes::OnBoard) ? WiFiRequestHandler::ON_BOARD_ANTENNA_PIN : WiFiRequestHandler::EXTERNAL_ANTENNA_PIN);
}

/**
 *  @brief Change the current antenna in use.
 * 
 *  Note that this does not change the actual default antenna in use, it just switches to a new one.
 *  Use SetAntenna followed by SwitchAntenna in order to change antenna that will be used as
 *  a default at startup.
 *
 *  @param newAntenna
 *      New antenna to use.
 */
void WiFiRequestHandler::SwitchAntenna(AntennaTypes::AntennaTypes newAntenna)
{
    ESP_LOGI(COMPONENT_NAME, "SwitchAntenna: Enter");
    //
    // TODO: What happens to any pending requests and do we disconnect WiFi first?
    //
    gpio_num_t newAntennaPin = ConvertAntennaToPin(newAntenna);
    gpio_num_t oldAntennaPin = ConvertAntennaToPin(static_cast<AntennaTypes::AntennaTypes>(GetAntenna()));
    //
    //  Make sure the old antenna is switched off before the new one is turned on.
    //
    gpio_set_level(oldAntennaPin, 0);
    gpio_set_level(newAntennaPin, 1);
    ESP_LOGI(COMPONENT_NAME, "SwitchAntenna; Exit");
}

/**
 *  @brief Setup the WiFi network class.
 */
void WiFiRequestHandler::Setup()
{
    ESP_LOGI(COMPONENT_NAME, "Setup: Enter");
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
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
    xTaskCreate(Task, COMPONENT_NAME, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
    ESP_LOGI(COMPONENT_NAME, "Setup: Exit");
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
    ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: Enter");
    if (eventBase == WIFI_EVENT)
    {
        switch (eventId)
        {
            case WIFI_EVENT_WIFI_READY:
                ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: WiFi Ready");
                break;
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: Scan for access points complete");
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT);
                break;
            case WIFI_EVENT_STA_START:
                ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: Station started");
                tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, SystemRequestHandler::GetDeviceName());
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_STATION_STARTED_BIT);
                break;
            case WIFI_EVENT_STA_STOP:
                ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: Station stopped");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: Station connected to access point");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                StationDisconnectedEvent(nullptr);
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
                    StationGotIpEvent();
                    break;
                case IP_EVENT_STA_LOST_IP:
                    ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: IP_EVENT_STA_LOST_IP");
                    break;
                case IP_EVENT_AP_STAIPASSIGNED:
                    ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: IP_EVENT_STA_LOST_IP");
                    break;
                case IP_EVENT_GOT_IP6:
                    ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: IP_EVENT_GOT_IP6");
                    break;
                case IP_EVENT_ETH_GOT_IP:
                    ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: IP_EVENT_ETH_GOT_IP");
                    break;
                case IP_EVENT_PPP_GOT_IP:
                    ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: IP_EVENT_PPP_GOT_IP");
                    break;
                case IP_EVENT_PPP_LOST_IP:
                    ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: IP_EVENT_PPP_LOST_IP");
                    break;
            }
        }
    }
    ESP_LOGI(COMPONENT_NAME, "EventHandlerHelper: Exit");
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
    ESP_LOGI(COMPONENT_NAME, "EventHandler: Enter");
    WiFiRequestHandler *sender = static_cast<WiFiRequestHandler *>(arg);
    sender->EventHandlerHelper(eventBase, eventId, eventData);
    ESP_LOGI(COMPONENT_NAME, "EventHandler: Exit");
}

/**
 *  @brief Dispatch the WiFi request to the appropriate method in the class.
 *
 *  @param request
 *      Data structure containing the request information from the sender.
 */
void WiFiRequestHandler::DispatchRequest(Message *request)
{
    ESP_LOGI(COMPONENT_NAME, "DispatchRequest: Enter");
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_BIT);
    switch (request->Function)
    {
        case WiFiFunction::StartNetwork:
            // _threadPool->Execute(WiFiRequestHandler::StartNetwork, (void *) request);
            StartNetwork(request);
            break;
        case WiFiFunction::Stop:
            Stop(request);
            break;
        case WiFiFunction::ConnectToAccessPoint:
            ConnectToAccessPoint(request);
            break;
        case WiFiFunction::DisconnectFromAccessPoint:
            DisconnectFromAccessPoint(request);
            break;
        case WiFiFunction::GetAccessPoints:
            GetAvailableNetworks(request);
            break;
        case WiFiFunction::Close:
            _threadPool->Execute(Close, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Read:
            _threadPool->Execute(Read, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::SetSockOpt:
            _threadPool->Execute(SetSockOpt, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Write:
            _threadPool->Execute(Write, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::FreeAddrInfo:
            _threadPool->Execute(FreeAddrInfo, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Connect:
            _threadPool->Execute(Connect, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Socket:
            _threadPool->Execute(Socket, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::GetAddrInfo:
            _threadPool->Execute(GetAddrInfo, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::SendTo:
            _threadPool->Execute(SendTo, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::RecvFrom:
            _threadPool->Execute(RecvFrom, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Poll:
            _threadPool->Execute(Poll, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Send:
            _threadPool->Execute(Send, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Accept:
            _threadPool->Execute(Accept, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Bind:
            _threadPool->Execute(Bind, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Listen:
            _threadPool->Execute(Listen, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::Ioctl:
            _threadPool->Execute(Ioctl, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        case WiFiFunction::GetSockName:
            _threadPool->Execute(GetSockName, (void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
            break;
        default:
            ESP_LOGI(COMPONENT_NAME, "DispatchRequest: Unknown WiFi message received, function %d", request->Function);
            if ((request->Payload != NULL) && (request->PayloadLength > 0))
            {
                ESP_LOG_BUFFER_HEXDUMP(COMPONENT_NAME, request->Payload, request->PayloadLength, ESP_LOG_INFO);
            }
            break;
    }
    xEventGroupWaitBits(_xWiFiEventGroup, WIFI_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(COMPONENT_NAME, "DispatchRequest: Exit");
}

/**
 *  @brief Disconnect from any access points.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::DisconnectFromAccessPoint(Message *message)
{
    ESP_LOGI(COMPONENT_NAME, "DisconnectFromAccessPoint: Enter");
    // TODO: WiFi - Disconnect from access point.
    ESP_LOGE(COMPONENT_NAME, "DisconnectFromAccessPoint: Disconnect function is not implemented yet.");
    ESP_LOGI(COMPONENT_NAME, "DisconnectFromAccessPoint: Exit");
}

/**
 *  @brief Stop the network and release all resources associated with any active connections.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::Stop(Message *message)
{
    ESP_LOGI(COMPONENT_NAME, "Stop: Enter");
    // TODO: WiFi - Stop network
    ESP_LOGE(COMPONENT_NAME, "Stop: Stop function is not implemented yet.");
    ESP_LOGI(COMPONENT_NAME, "Stop: Exit");
}

/**
 *  @brief Connect to a specified access point.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ConnectToAccessPoint(Message *message)
{
    ESP_LOGI(COMPONENT_NAME, "ConnectToAccessPoint: Enter");
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    _currentRetryCount = 0;
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    {
        if (StartWiFiInterface() != ESP_OK)
        {
            result = StatusCodes::CannotStartNetworkInterface;
        }
    }

    Esp32Messaging::WiFiCredentials *credentials = Encoders::ExtractWiFiCredentials(message->Payload);
    message->DeletePayload();
    if (result == StatusCodes::CompletedOk)
    {
        _currentRetryCount = 0;
        if ((strlen(credentials->NetworkName) > MAXIMUM_SSID_LENGTH) || (strlen(credentials->Password) > MAXIMUM_PASSWORD_LENGTH))
        {
            result = StatusCodes::InvalidWiFiCredentials;
            ESP_LOGI(COMPONENT_NAME, "ConnectToAccessPoint: Invalid credentials");
        }
        else
        {
            wifi_config_t wifi_config = {};
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifi_config.sta.pmf_cfg =
            {
                .capable = true,
                .required = false
            };
            memcpy(wifi_config.sta.ssid, credentials->NetworkName, strlen(credentials->NetworkName));
            memcpy(wifi_config.sta.password, credentials->Password, strlen(credentials->Password));
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
            ESP_ERROR_CHECK(esp_wifi_start());

            bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_STATION_STARTED_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
            if (bits & WIFI_STATION_STARTED_BIT)
            {
                ESP_ERROR_CHECK(esp_wifi_connect());

                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
                if (bits & WIFI_CONNECTED_BIT)
                {
                    esp_netif_ip_info_t ipInfo;
                    ESP_ERROR_CHECK(esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo));
                    message->PayloadLength = 3 * sizeof(esp_ip4_addr_t);
                    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
                    memcpy(message->Payload, (void *) &ipInfo.ip, sizeof(esp_ip4_addr_t));
                    memcpy(message->Payload + sizeof(esp_ip4_addr_t), (void *) &ipInfo.netmask, sizeof(esp_ip4_addr_t));
                    memcpy(message->Payload + 2 * (sizeof(esp_ip4_addr_t)), (void *) &ipInfo.gw, sizeof(esp_ip4_addr_t));
                    if (GetGetTimeAtStartup())
                    {
                        //
                        //  Note that we only need to get the time here.  The timezone is set when this
                        //  class is instanciated or when the SetTimezone method is successfully called.
                        //
                        sntp_setoperatingmode(SNTP_OPMODE_POLL);
                        sntp_setservername(0, GetNtpServer());
                        sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
                        sntp_init();
                        bool gettingTime = true;
                        int gettingTimeCount = 0;
                        while (gettingTime)
                        {
                            gettingTimeCount++;
                            if (gettingTimeCount == 10)     // Restrict number of tries to 1 second.
                            {
                                gettingTimeCount = false;
                            }
                            else
                            {
                                vTaskDelay(100 / portTICK_PERIOD_MS);
                                gettingTime = (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED);
                            }
                        }
                    }
                }
                else
                {
                    result = StatusCodes::Timeout;
                }
            }
            else
            {
                result = StatusCodes::Timeout;
            }
        }        
    }
    vPortFree(credentials);

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
    ESP_LOGI(COMPONENT_NAME, "ConnectToAccessPoint: Exit");
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
    ESP_LOGI(COMPONENT_NAME, "StartWiFiInterface: Enter");
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
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT);
    }
    ESP_LOGI(COMPONENT_NAME, "StartWiFiInterface: Exit");
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
// void WiFiRequestHandler::StartNetwork(void *m)
void WiFiRequestHandler::StartNetwork(Message *message)
{
    ESP_LOGI(COMPONENT_NAME, "StartNetwork: Enter");
    message->MessageType = MessageTypes::Response;
    if (StartWiFiInterface() == ESP_OK)
    {
        message->StatusCode = StatusCodes::CompletedOk;
    }
    else
    {
        message->StatusCode = StatusCodes::Failure;
    }
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
    ESP_LOGI(COMPONENT_NAME, "StartNetwork: Exit");
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
    ESP_LOGI(COMPONENT_NAME, "GetAvailableNetworks: Enter");
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    _currentRetryCount = 0;
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    {
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
            ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

            bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
            if (bits & WIFI_SCAN_COMPLETE_BIT)
            {
                uint16_t apCount = 0;
                esp_wifi_scan_get_ap_num(&apCount);
                ESP_LOGI(COMPONENT_NAME, "GetAvailableNetworks: Number of access points found: %d", apCount);
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
                ESP_LOGI(COMPONENT_NAME, "GetAvailableNetworks: Scan timeout.");
                result = StatusCodes::Timeout;
            }
            vPortFree(ap_info);
        }
        
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
    ESP_LOGI(COMPONENT_NAME, "GetAvailableNetworks: Exit");
}

/**
 *  @brief Expose the POSIX getaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getaddrinfo method.
 */
void WiFiRequestHandler::GetAddrInfo(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "GetAddrInfo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::GetAddrInfoRequest *request = Encoders::ExtractGetAddrInfoRequest(message->Payload);
    //
    Esp32Messaging::AddrInfo *extractedHints = Encoders::ExtractAddrInfo(request->Hints);
    struct addrinfo hints = {};
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

    Esp32Messaging::GetAddrInfoResponse *response = static_cast<Esp32Messaging::GetAddrInfoResponse *>(pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoResponse)));
    response->AddrInfoResponseErrno = errorCode;
    if ((errorCode == 0) || (addressInformation != NULL))
    {
        Esp32Messaging::SockAddr sa = {};
        Esp32Messaging::AddrInfo a = {};

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
        response->Res = nullptr;
        response->ResLength = 0;
        vPortFree(addressInformation);
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
    ESP_LOGI(COMPONENT_NAME, "GetAddrInfo: Exit");
}

/**
 *  @brief Expose the POSIX socket method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the socket method.
 */
void WiFiRequestHandler::Socket(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Socket: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::SocketRequest *request = Encoders::ExtractSocketRequest(message->Payload);
    ESP_LOGI(COMPONENT_NAME, "Socket: Creating socket on domain %d, type %d and protocol %d", request->Domain, request->Type, request->Protocol);

    Esp32Messaging::IntegerResponse response;
    response.Result = socket(request->Domain, request->Type, request->Protocol);
    ESP_LOGI(COMPONENT_NAME, "Socket: Created socket %d", response.Result);

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Socket: Exit");
}

/**
 *  @brief Expose the POSIX connect method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the connect method.
 */
void WiFiRequestHandler::Connect(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Connect: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::ConnectRequest *request = Encoders::ExtractConnectRequest(message->Payload);
    Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);
    Esp32Messaging::IntegerAndErrnoResponse response;
    response.Result = 0;

    struct sockaddr_in sa = {};
    int family;
    Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
    errno = 0;
    if (family == AF_INET)
    {
        sa.sin_family = (uint8_t) (family & 0xff);
        sa.sin_port = socketAddress->Port;
        memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(sa.sin_addr));
        sa.sin_len = sizeof(struct sockaddr_in);
        ESP_LOGI(COMPONENT_NAME, "Connect: Connecting socket: %d", request->SocketHandle);
        ESP_LOGI(COMPONENT_NAME, "Connect: Family: %d", sa.sin_family);
        ESP_LOGI(COMPONENT_NAME, "Connect: IP address: %s", inet_ntoa(sa.sin_addr));
        ESP_LOGI(COMPONENT_NAME, "Connect: Port: %d", ntohs(sa.sin_port));
        response.Result = connect(request->SocketHandle, (struct sockaddr *) &sa, sizeof(sa));
    }
    else
    {
        response.Result = -1;
        errno = EFAULT;
    }

    if (response.Result != 0)
    {
        int32_t result = 0;
        ESP_LOGI(COMPONENT_NAME, "Connect: Error code: %d - %s", errno, strerror(errno));
        if (Mapping::GetStmValue(Mapping::Errno, errno, &result) != 0)
        {
            Mapping::GetStmValue(Mapping::Errno, EFAULT, &result);
        }
        response.ResponseErrno = result;
        response.Result = -1;
    }
    else
    {
        response.ResponseErrno = 0;
    }

    vPortFree(request);
    vPortFree(socketAddress);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Connect: Exit");
}

/**
 *  @brief Expose the POSIX freeaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the freeaddrinfo method.
 */
void WiFiRequestHandler::FreeAddrInfo(void  *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "FreeAddrInfo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    ESP_LOGI(COMPONENT_NAME, "FreeAddrInfo: Releasing addrinfo storage.");
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
    ESP_LOGI(COMPONENT_NAME, "FreeAddrInfo: Exit");
}

/**
 *  @brief Expose the POSIX setsockopt method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the setsockopt method.
 */
void WiFiRequestHandler::SetSockOpt(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::SetSockOptRequest *request = Encoders::ExtractSetSockOptRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response;

    void *optionValue = nullptr;
    int optionLength = 0;
    Esp32Messaging::TimeVal *tv = nullptr;
    struct timeval timeOut;

    int32_t optionName = 0;
    if (Mapping::GetEspValue(Mapping::SocketOptions, request->OptionName, &optionName) < 0)
    {
        ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Unknown option: %d", request->OptionName);
        response.Result = -1;
        errno = ENOPROTOOPT;
    }
    else
    {
        errno = 0;
        switch (optionName)
        {
            case SO_SNDTIMEO:       /* Set the send timeout */
            case SO_RCVTIMEO:       /* Set the receive timeout */
                tv = Encoders::ExtractTimeVal(request->OptionValue);
                timeOut.tv_sec = tv->TvSec;
                timeOut.tv_usec = tv->TvUsec;
                vPortFree(tv);
                optionValue = &timeOut;
                optionLength = sizeof(timeOut);
                break;
            case SO_RCVBUF:         /* Set the receive buffer size */
            case SO_REUSEADDR:      /* Set validation rules for bind */
                optionValue = request->OptionValue;
                optionLength = request->OptionValueLength;
                break;
            case SO_LINGER:         /* Linger on close if data present */
                ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Implement SO_LINGER");
                break;
            case SO_ACCEPTCONN:     /* Socket has had listen() */
                ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Implement SO_ACCEPTCONN");
                break;
            case SO_DONTLINGER:     /* Don't linger on close if data present */
                ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Implement SO_DONTLINGER");
                break;
            case SO_ERROR:          /* Get error status and clear */
                ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Implement SO_ERROR");
                break;
            case SO_TYPE:           /* Get socket type */
                ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Implement SO_TYPE");
                break;
            case SO_NO_CHECK:       /* Don't create UDP checksum */
                ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Implement SO_NO_CHECK");
                break;
            default:
                response.Result = -1;
                errno = ENOPROTOOPT;
                break;
        }
        if (errno == 0)
        {
            int32_t level = 0;
            Mapping::GetEspValue(Mapping::SocketLevel, request->Level, &level);
            ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Setting socket option level to 0x%x, option name 0x%x for socket %d", level, optionName, request->SocketHandle);
            response.Result = setsockopt(request->SocketHandle, level, optionName, optionValue, optionLength);
        }
    }    

    if (response.Result < 0)
    {
        int32_t e = 0;
        if (Mapping::GetStmValue(Mapping::Errno, errno, &e) != 0)
        {
            Mapping::GetStmValue(Mapping::Errno, EFAULT, &e);
        }
        response.ResponseErrno = e;
    }
    else
    {
        response.ResponseErrno = 0;
    }
    ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "SetSockOpt: Exit");
}

/**
 *  @brief Expose the POSIX write method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the write method.
 */
void WiFiRequestHandler::Write(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Write: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::WriteRequest *request = Encoders::ExtractWriteRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response;

    response.Result = write(request->SocketHandle, request->Buffer, request->Count);
    if (response.Result < 0)
    {
        int32_t e = 0;
        if (Mapping::GetStmValue(Mapping::Errno, errno, &e) != 0)
        {
            Mapping::GetStmValue(Mapping::Errno, EFAULT, &e);
        }
        response.ResponseErrno = e;
    }
    else
    {
        response.ResponseErrno = 0;
    }

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Write: Exit");
}

/**
 *  @brief Expose the POSIX read method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the read method.
 */
void WiFiRequestHandler::Read(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Read: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::ReadRequest *request = Encoders::ExtractReadRequest(message->Payload);
    Esp32Messaging::ReadResponse response;

    response.Buffer = static_cast<uint8_t *>(pvPortMalloc(request->Count));
    bzero(response.Buffer, request->Count);
    response.BufferLength = request->Count;
    response.ReadResponseResult = read(request->SocketHandle, response.Buffer, request->Count);
    if (response.ReadResponseResult < 0)
    {
        int32_t e = 0;
        if (Mapping::GetStmValue(Mapping::Errno, errno, &e) != 0)
        {
            Mapping::GetStmValue(Mapping::Errno, EFAULT, &e);
        }
        response.ReadResponseErrno = e;
    }
    else
    {
        response.ReadResponseErrno = 0;
    }

    vPortFree(request);
    vPortFree(message->Payload);

    //
    //  TODO: Optimise this by not sending an empty buffer in the case of an error.
    //
    message->PayloadLength = Encoders::EncodedReadResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeReadResponse(&response, message->Payload);

    vPortFree(response.Buffer);

    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Read: Exit");
}

/**
 *  @brief Expose the POSIX close method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the close method.
 */
void WiFiRequestHandler::Close(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Close: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::CloseRequest *request = Encoders::ExtractCloseRequest(message->Payload);

    Esp32Messaging::IntegerAndErrnoResponse response;
    response.Result = close(request->SocketHandle);
    if (response.Result != 0)
    {
        if (response.Result < 0)
        {
            int32_t e = 0;
            if (Mapping::GetStmValue(Mapping::Errno, errno, &e) != 0)
            {
                Mapping::GetStmValue(Mapping::Errno, EFAULT, &e);
            }
            response.Result = e;
        }
    }

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Close: Exit");
}

/**
 *  @brief Expose the POSIX send method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the send method.
 */
void WiFiRequestHandler::Send(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Send: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::SendRequest *request = Encoders::ExtractSendRequest(message->Payload);
    vPortFree(message->Payload);

    Esp32Messaging::IntegerAndErrnoResponse response;
    int32_t mappedFlags = 0;
    response.ResponseErrno = 0;
    response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
    if (response.Result == 0)
    {
        response.Result = send(request->SocketHandle, request->Buffer, request->Length, mappedFlags);
        if (response.Result < 0)
        {
            if (Mapping::GetStmValue(Mapping::Errno, errno, &response.ResponseErrno) != 0)
            {
                Mapping::GetStmValue(Mapping::Errno, EFAULT, &response.ResponseErrno);
            }
        }
    }
    else
    {
        Mapping::GetStmValue(Mapping::Errno, EFAULT, &response.ResponseErrno);
    }

    ESP_LOGI(COMPONENT_NAME, "Send: Sent %d bytes.", request->Length);
    if (request->Buffer && (request->Length > 0))
    {
        ESP_LOG_BUFFER_HEX(COMPONENT_NAME, request->Buffer, request->Length);
    }
    ESP_LOGI(COMPONENT_NAME, "Send: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));

    vPortFree(request->Buffer);
    vPortFree(request);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Send: Exit");
}

/**
 *  @brief Expose the POSIX sendto method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the sendto method.
 */
void WiFiRequestHandler::SendTo(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "SendTo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::SendToRequest *request = Encoders::ExtractSendToRequest(message->Payload);

    Esp32Messaging::IntegerAndErrnoResponse response;
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
            ESP_LOGI(COMPONENT_NAME, "SendTo: Mapping request family %d to ESP family %d", socketAddress->Family, family);
        }

        if (family == AF_INET)
        {
            struct sockaddr_in sa = {};
            struct sockaddr *psa = NULL;
            size_t lenSa = 0;
            if (request->DestinationAddress)
            {
                sa.sin_family = (unsigned char) (family & 0xff);

                sa.sin_port = socketAddress->Port;
                memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(sa.sin_addr));
                sa.sin_len = sizeof(sa);
                psa = (struct sockaddr *) &sa;
                lenSa = sizeof(sa);
            }
            ESP_LOGI(COMPONENT_NAME, "SendTo: Connecting socket: %d", request->SocketHandle);
            ESP_LOGI(COMPONENT_NAME, "SendTo: Family: %d", (psa == NULL) ? 0 : sa.sin_family);
            ESP_LOGI(COMPONENT_NAME, "SendTo: IP address: %s", (psa == NULL) ? "NULL" : inet_ntoa(sa.sin_addr));
            ESP_LOGI(COMPONENT_NAME, "SendTo: Port: %d", (psa == NULL) ? 0 : ntohs(sa.sin_port));
            response.Result = sendto(request->SocketHandle, request->Buffer, request->Length, mappedFlags, psa, lenSa);
        }

        if (socketAddress != NULL)
        {
            vPortFree(socketAddress);
        }
        
        if (response.Result < 0)
        {
            if (Mapping::GetStmValue(Mapping::Errno, errno, &response.ResponseErrno) != 0)
            {
                Mapping::GetStmValue(Mapping::Errno, EFAULT, &response.ResponseErrno);
            }
        }
    }
    else
    {
        Mapping::GetStmValue(Mapping::Errno, EFAULT, &response.ResponseErrno);
    }

    ESP_LOGI(COMPONENT_NAME, "SendTo: Sent %d bytes.", request->Length);
    if (request->Buffer && (request->Length > 0))
    {
        ESP_LOG_BUFFER_HEX(COMPONENT_NAME, request->Buffer, request->Length);
    }
    ESP_LOGI(COMPONENT_NAME, "SendTo: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));

    vPortFree(request->Buffer);
    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "SendTo: Exit");
}

/**
 *  @brief Expose the POSIX recvfrom method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the recvfrom method.
 */
void WiFiRequestHandler::RecvFrom(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "RecvFrom: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::RecvFromRequest *request = Encoders::ExtractRecvFromRequest(message->Payload);

    Esp32Messaging::RecvFromResponse response = {};
    int32_t mappedFlags = 0;
    response.ResponseErrno = 0;
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
            if (Mapping::GetStmValue(Mapping::Errno, errno, &response.ResponseErrno) != 0)
            {
                Mapping::GetStmValue(Mapping::Errno, EFAULT, &response.ResponseErrno);
            }
        }
        else
        {
            if (request->GetSourceAddress != 0)
            {
                Esp32Messaging::SockAddr socketAddress = {};
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
        Mapping::GetStmValue(Mapping::Errno, EFAULT, &response.ResponseErrno);
    }
    ESP_LOGI(COMPONENT_NAME, "RecvFrom: Received %d bytes.", response.Result);
    if (response.Buffer && (response.BufferLength > 0))
    {
        ESP_LOG_BUFFER_HEX(COMPONENT_NAME, response.Buffer, response.Result);
    }
    ESP_LOGI(COMPONENT_NAME, "RecvFrom: Result: %d, errno: %d - %s", response.Result, response.ResponseErrno, strerror(response.ResponseErrno));

    vPortFree(request);
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
    ESP_LOGI(COMPONENT_NAME, "RecvFrom: Exit");
}

/**
 *  @brief Expose the POSIX bind method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the bind method.
 */
void WiFiRequestHandler::Bind(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Bind: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::BindRequest *request = Encoders::ExtractBindRequest(message->Payload);
    Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);
    Esp32Messaging::IntegerAndErrnoResponse response;
    response.Result = 0;

    struct sockaddr_in sa = {};
    int family;
    Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
    errno = 0;
    if (family == AF_INET)
    {
        sa.sin_family = socketAddress->Family;
        sa.sin_port = socketAddress->Port;
        memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(socketAddress->Ip4Address));
        ESP_LOGI(COMPONENT_NAME, "SBind: ocket handle: %d", request->SocketHandle);
        ESP_LOGI(COMPONENT_NAME, "FBind: amily: %d", sa.sin_family);
        ESP_LOGI(COMPONENT_NAME, "Bind: IP address: %s", inet_ntoa(sa.sin_addr));
        ESP_LOGI(COMPONENT_NAME, "Bind: Port: %d", ntohs(sa.sin_port));
        response.Result = bind(request->SocketHandle, (struct sockaddr *) &sa, sizeof(sa));
    }
    else
    {
        response.Result = -1;
        errno = EFAULT;
    }

    if (response.Result != 0)
    {
        int32_t result = 0;
        if (Mapping::GetStmValue(Mapping::Errno, errno, &result) != 0)
        {
            Mapping::GetStmValue(Mapping::Errno, EFAULT, &result);
        }
        response.ResponseErrno = result;
        response.Result = -1;
    }
    else
    {
        response.ResponseErrno = 0;
    }
    ESP_LOGI(COMPONENT_NAME, "Bind: Error code: %d - %s", errno, strerror(errno));
    vPortFree(request);
    vPortFree(socketAddress);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Bind: Exit");
}

/**
 *  @brief Expose the POSIX listen method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the listen method.
 */
void WiFiRequestHandler::Listen(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Listen: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::ListenRequest *request = Encoders::ExtractListenRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response;

    response.Result = listen(request->SocketHandle, request->BackLog);
    response.ResponseErrno = (response.Result == 0) ? 0 : errno;

    vPortFree(request);

    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    PERFORMANCE_LOGGING_DUMP_RESULTS();
    ESP_LOGI(COMPONENT_NAME, "Listen: Exit");
}

/**
 *  @brief Expose the POSIX accept method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the accept method.
 */
void WiFiRequestHandler::Accept(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Accept: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::AcceptRequest *request = Encoders::ExtractAcceptRequest(message->Payload);
    free(message->Payload);
    Esp32Messaging::AcceptResponse response;

    struct sockaddr sa = {};
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
        ESP_LOGI(COMPONENT_NAME, "Accept: Socket handle: %d", response.Result);
        ESP_LOGI(COMPONENT_NAME, "Accept: Family: %d", sin->sin_family);
        ESP_LOGI(COMPONENT_NAME, "Accept: IP address: %s", inet_ntoa(sin->sin_addr));
        ESP_LOGI(COMPONENT_NAME, "Accept: Port: %d", ntohs(sin->sin_port));
    }
    else
    {
        response.AddrLength = 0;
        response.Addr = nullptr;
        int32_t result = 0;
        if (Mapping::GetStmValue(Mapping::Errno, errno, &result) != 0)
        {
            Mapping::GetStmValue(Mapping::Errno, EFAULT, &result);
        }
        response.ResponseErrno = result;
    }
    vPortFree(request);

    message->PayloadLength = Encoders::EncodedAcceptResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeAcceptResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Accept: Exit");
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
    ESP_LOGI(COMPONENT_NAME, "Ioctl: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IoctlRequest *request = Encoders::ExtractIoctlRequest(message->Payload);
    vPortFree(message->Payload);
    message->PayloadLength = 0;

    Esp32Messaging::IoctlResponse response;
    switch (request->Command)
    {
        case SIOCGIFCONF:
            esp_netif_ip_info_t ipInfo;
            if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
            {
                Esp32Messaging::SockAddr sa = {};
                int family;

                Mapping::GetStmValue(Mapping::AddressFamily, AF_INET, &family);
                sa.Family = (unsigned char) (family & 0xff);
                memcpy(&sa.Ip4Address, &ipInfo.ip, sizeof(ipInfo.ip));
                response.AddrLength = Encoders::EncodedSockAddrBufferSize(&sa);
                response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
                Encoders::EncodeSockAddr(&sa, response.Addr);
            }
            break;
        case SIOCGIFFLAGS:
            if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
            {
                response.Flags = IFF_UP;
            }
            else
            {
                response.Flags = IFF_DOWN;
            }
            response.AddrLength = 0;
            response.Addr = nullptr;
            break;
    }
    vPortFree(request);

    message->PayloadLength = Encoders::EncodedIoctlResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIoctlResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "Ioctl: Exit");
}

/**
 *  @brief Expose the POSIX getsockname method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockname method.
 */
void WiFiRequestHandler::GetSockName(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "GetSockName: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::GetSockNameRequest *request = Encoders::ExtractGetSockNameRequest(message->Payload);
    free(message->Payload);
    Esp32Messaging::GetSockNameResponse response;

    struct sockaddr sa = {};
    socklen_t saLen = sizeof(sa);
    response.Result = getsockname(request->SocketHandle, &sa, &saLen);
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
        ESP_LOGI(COMPONENT_NAME, "GetSockName: Socket handle: %d", response.Result);
        ESP_LOGI(COMPONENT_NAME, "GetSockName: Family: %d", sin->sin_family);
        ESP_LOGI(COMPONENT_NAME, "GetSockName: IP address: %s", inet_ntoa(sin->sin_addr));
        ESP_LOGI(COMPONENT_NAME, "GetSockName: Port: %d", ntohs(sin->sin_port));
    }
    else
    {
        response.AddrLength = 0;
        response.Addr = nullptr;
        int32_t result = 0;
        if (Mapping::GetStmValue(Mapping::Errno, errno, &result) != 0)
        {
            Mapping::GetStmValue(Mapping::Errno, EFAULT, &result);
        }
        response.ResponseErrno = result;
    }
    vPortFree(request);

    message->PayloadLength = Encoders::EncodedGetSockNameResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeGetSockNameResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    ESP_LOGI(COMPONENT_NAME, "GetSockName: Exit");
}

/**
 *  @brief Expose the poll method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
void WiFiRequestHandler::Poll(void *vpMessage)
{
    ESP_LOGI(COMPONENT_NAME, "Poll: Enter");
    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::PollRequest *request = Encoders::ExtractPollRequest(message->Payload);
    // PollRequestInformation *pollRequest = static_cast<PollRequestInformation *>(pvPortMalloc(sizeof(PollRequestInformation)));
    Esp32Messaging::PollResponse response;

    bzero(&response, sizeof(Esp32Messaging::PollResponse));
    response.Result = 0;
    errno = 0;
    int event = 0;
    if (request->Setup == 2)
    {
        struct pollfd fds = {};
        fds.fd = request->SocketHandle;
        Mapping::GetEspFlagValue(Mapping::PollEvents, request->Events, &event);
        ESP_LOGI(COMPONENT_NAME, "Poll: Requested Nuttx events 0x%04d maps to ESP events 0x%04d", request->Events, event);
        fds.events = (uint16_t) (event & 0xffff);

        response.Result = poll(&fds, 1, request->Timeout);
        // response.Result = poll(&fds, 1, request->Timeout);
        ESP_LOGI(COMPONENT_NAME, "Poll: Returned value: %d", response.Result);
        if (response.Result > 0)
        {
            Mapping::GetStmFlagValue(Mapping::PollEvents, fds.revents, &event);
            response.ReturnedEvents = (uint16_t) (event & 0xffff);
        }
        ESP_LOGI(COMPONENT_NAME, "Poll: Returned ESP32 events 0x%04d maps to NuttX events 0x%04d", fds.revents, event);
    }
    else
    {
        ESP_LOGI(COMPONENT_NAME, "Poll: Poll request not supported.");
        errno = EFAULT;
    }
    
    response.ResponseErrno = errno;
    if (errno != 0)
    {
        Mapping::GetStmValue(Mapping::Errno, errno, &response.ResponseErrno);
        response.Result = -1;
    }

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedPollResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodePollResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);
    ESP_LOGI(COMPONENT_NAME, "Poll: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Event Handlers
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief The access point has just given the ESP32 an IP address.
 * 
 *  This event indicates that we have successfully connected to the access point.
 */
void WiFiRequestHandler::StationGotIpEvent()
{
    ESP_LOGI(COMPONENT_NAME, "StationGotIpEvent: Enter");
    _currentRetryCount = 0;
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);
    ESP_LOGI(COMPONENT_NAME, "StationGotIpEvent: Exit");
}

/**
 *  @brief Process the WiFi disconnected event.
 *
 *  @param eventData
 *      Data for the disconnected event.
 */
void WiFiRequestHandler::StationDisconnectedEvent(system_event_t *eventData)
{
    if (_automaticReconnect)
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_BIT | WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
        if (_currentRetryCount < _maximumRetryCount)
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_RECONNECTING_BIT);
            esp_wifi_connect();
            _currentRetryCount++;
        }
        else
        {
            //    TODO: Deal with retry counts exceeded in automatic reconnect mode.
        }
    }
    else
    {
        //
        //  TODO: Send disconnect message ?
        //
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 *                     Poll Request Task Related Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Compare the message ID in a message from the STM32 with the specified message ID.
 * 
 *  @param messageId
 *      Message ID being looked for.
 * 
 *  @param event
 *      A message from the STM32 containing the original request.
 * 
 *  @returns
 *      True if the message IDs match, false otherwise.
 */
bool WiFiRequestHandler::ComparePollRequestMessageIds(uint32_t messageId, PollRequestInformation *request)
{
    return(messageId == request->OriginalRequestID);
}

/**
 *  @brief Task that will process a poll request.
 * 
 *  @param parameters
 *      Pointer to a PollRequest object containing the information about
 *      request along with any management data to support the request.
 */
void WiFiRequestHandler::PollRequestProcessor(void *parameters)
{
    ESP_LOGI(COMPONENT_NAME, "PollRequestProcessor: Enter");
    PollRequestInformation *request = static_cast<PollRequestInformation *>(parameters);
    xSemaphoreTake(_pollRequestListMutex, portMAX_DELAY);
    _pollRequests->AddToHead(request);
    xSemaphoreGive(_pollRequestListMutex);

    request->PollResult = 0;   // Just in case we terminate the task.
    request->PollErrno = 0;
    request->Fds->revents = 0;
    request->PollResult = poll(request->Fds, 1, request->Timeout);
    if (request->PollResult < 0)
    {
        Mapping::GetStmValue(Mapping::Errno, errno, &request->PollErrno);
    }
    else
    {
        int event = 0;
        Mapping::GetStmFlagValue(Mapping::PollEvents, request->Fds->revents, &event);
        ESP_LOGI(COMPONENT_NAME, "PollRequestProcessor: Mapping ESP returned events 0x%04x to Nuttx returned events 0x%04x", request->Fds->revents, event);
        request->Fds->revents = (uint16_t) (event & 0xffff);
    }
    request->Teardown = false;
    if (xQueueSendToBack(_completedPollRequestsQueue, &request, 0) != pdPASS)
    {
        ESP_LOGE(COMPONENT_NAME, "PollRequestProcessor: Cannot queue completed poll message.");
    }

    xSemaphoreTake(_pollRequestListMutex, portMAX_DELAY);
    _pollRequests->RemoveItem(request->OriginalRequestID);
    xSemaphoreGive(_pollRequestListMutex);
    ESP_LOGI(COMPONENT_NAME, "PollRequestProcessor: Exit");
    //
    //  This method should never exit.  It will be cleaned up from another task
    //  using a vTaskDelete call.
    //
    vTaskSuspend(NULL);
}

/**
 *  @brief Deal with completed poll requests.
 * 
 *  @param parameters
 *      This should be NULL as all process information will come through
 *      the completed poll requests queue.
 */
void WiFiRequestHandler::CompletedPollRequestProcessor(void *parameters)
{
    while (true)
    {
        PollRequestInformation *request;
        BaseType_t result = xQueueReceive(_completedPollRequestsQueue, &request, portMAX_DELAY);
        if (result == pdTRUE)
        {
            if (!request->Teardown)
            {
                ESP_LOGI(COMPONENT_NAME, "CompletedPollRequestProcessor: Queuing response for poll task 0x%08x", request->OriginalRequestID);
                //
                //  Prepare the response payload.
                //
                Esp32Messaging::InterruptPollResponse response;
                response.ReturnedEvents = request->Fds->revents;
                response.SocketHandle = request->Fds->fd;
                response.Result = request->PollResult;
                response.ResponseErrno = request->PollErrno;
                response.SetupMessageId = request->OriginalRequestID;
                //  
                //  Use tha original message information and queue the response.
                //
                Message *message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));
                message->Interface = Esp32Interfaces::WiFi;
                message->Function = WiFiFunction::InterruptPollResponse;
                message->MessageID = _messageDispatcher->GetNextMessageID();
                message->PayloadLength = Encoders::EncodedInterruptPollResponseBufferSize(&response);
                message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
                Encoders::EncodeInterruptPollResponse(&response, message->Payload);
                message->MessageType = MessageTypes::Interrupt;
                message->StatusCode = StatusCodes::CompletedOk;
                _messageDispatcher->QueueMessageForStm32(message);
            }
            //
            //  Clean up.
            //
            ESP_LOGI(COMPONENT_NAME, "CompletedPollRequestProcessor: Terminating poll task 0x%08x", request->OriginalRequestID);
            vTaskDelete(request->TaskHandle);
            vPortFree(request);
        }
    }
}