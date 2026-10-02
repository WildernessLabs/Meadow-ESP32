/**
 * @file WiFiRequestHandler.cpp
 * @author Mark Stevens
 * @brief Provide WiFi specific methods and properties.
 * @date 2019-02-03
 *
 * @copyright Copyright (c) 2019 Wilderness Labs
 */

#include "sdkconfig.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lwip/apps/sntp.h"
#include <inttypes.h>

#include "Globals.hpp"
#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "Exceptions/ThreadPoolException.hpp"
#include "Mapping.hpp"
#include "Logging.hpp"
#include "ProgressiveDelay.hpp"
#include "Utils.hpp"
#include "MutexLock.hpp"
#include "SystemRequestHandler.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

const char *WiFiRequestHandler::COMPONENT_NAME = "WiFi";

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
std::map<uint32_t, struct pollfd *> WiFiRequestHandler::_pollRequests;

/**
 *  @brief Mutex for the poll request linked list.
 */
SemaphoreHandle_t WiFiRequestHandler::_pollRequestListMutex = nullptr;

/**
 *  @brief Socket handles the STM32 has been given, mapped to the lwIP descriptors behind them.
 */
std::map<int32_t, int> WiFiRequestHandler::_socketHandles;

/**
 *  @brief Mutex for _socketHandles.
 */
SemaphoreHandle_t WiFiRequestHandler::_socketHandlesMutex = nullptr;

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
std::atomic<esp_netif_t *> WiFiRequestHandler::_stationNetIfHandle(nullptr);

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
std::atomic<bool> WiFiRequestHandler::_automaticReconnect(false);

/**
 *  @brief Name of the NVS storage holding the _automaticReconnect value.
 */
const char *WiFiRequestHandler::AUTOMATIC_RECONNECT_NAME = "AutoReconnect";

/**
 *  @brief Maximum number of times a failed event should retry before raising an error.
 */
std::atomic<uint32_t> WiFiRequestHandler::_maximumRetryCount(WiFiRequestHandler::DEFAULT_MAXIMUM_RETRY_COUNT);

/**
 *  @brief Name of the NVS storage holding the _maximumRetryCount value.
 */
const char *WiFiRequestHandler::MAXIMUM_RETRY_COUNT_NAME = "MaxRetry";

/**
 *  @brief Indicate if the network should start automatically at start up.
 */
bool WiFiRequestHandler::_automaticallyStartNetwork = false;

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
std::atomic<bool> WiFiRequestHandler::_useDhcp(false);

/**
 *  @brief Name of the storage holding the _useDhcp configuration setting.
 */
const char *WiFiRequestHandler::USE_DHCP_NAME = "UseDHCP";

/**
 *  @brief Default access point to be used when automatically connecting.
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
 *  @brief Mutex guarding _defaultAccessPoint and _password.
 *
 *  A function-local static is created exactly once, on first use, even when several tasks
 *  get here at the same time, so no caller depends on the handler having been constructed.
 *
 *  @returns
 *      The credentials mutex.
 */
SemaphoreHandle_t WiFiRequestHandler::CredentialsMutex()
{
    static const SemaphoreHandle_t mutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the WiFi credentials mutex");
    return (mutex);
}

/**
 *  @brief Name of the storage in NVS holding the password for the access point.
 */
const char *WiFiRequestHandler::PASSWORD_NAME = "Password";

/**
 *  @brief List of valid country codes.
 *
 *  @note The first element of this array is used as the default country code if no value is found in NVS.
 *        It is important that this code is "01" as this is the code that the ESP32 recognises as the default
 *        world wide code that is safe for all regions.
 */
const char * const WiFiRequestHandler::_validCountryCodes[] = {"01", "AT", "AU", "BE", "BG", "BR", "CA", "CH", "CN", "CY", "CZ", "DE", "DK", "EE", "ES", "FI", "FR", "GB", "GR", "HK", "HR", "HU", "IE",
                                                               "IN", "IS", "IT", "JP", "KR", "LI", "LT", "LU", "LV", "MT", "MX", "NL", "NO", "NZ", "PL", "PT", "RO", "SE", "SI", "SK", "TW", "US"};

/**
 * @brief Name of the storage in NVS holding the country code.
 */
const char *WiFiRequestHandler::COUNTRY_CODE_NAME = "CountryCode";

/**
 * @brief Country code for the WiFi configuration.
 */
std::atomic<const char *> WiFiRequestHandler::_countryCode(nullptr);

/**
 *  @brief Name of the NTP server to be used when using NTP to set the time
 *  when the board starts.
 */
const char *WiFiRequestHandler::_ntpServer = "pool.ntp.org";

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
const char *WiFiRequestHandler::_timezone = "UTC";

/**
 *  @brief Static IP address when not using DHCP.
 */
std::atomic<uint32_t> WiFiRequestHandler::_staticIpAddress(0);

/**
 *  @brief Name of the storage space holding the static IP address.
 */
const char *WiFiRequestHandler::STATIC_IP_ADDRESS_NAME = "IpAddress";

/**
 *  @brief Static subnet mask when not using DHCP.
 */
std::atomic<uint32_t> WiFiRequestHandler::_staticSubNetMask(0);

/**
 *  @brief Name of the storage space holding the static subnet mask.
 */
const char *WiFiRequestHandler::STATIC_SUBNET_MASK_NAME = "SubnetMask";

/**
 *  @brief DNS server when not using DHCP.
 */
std::atomic<uint32_t> WiFiRequestHandler::_dnsServer(0);

/**
 *  @brief Name of the storage in NVS holding the DNS server IP address.
 */
const char *WiFiRequestHandler::DNS_SERVER_NAME = "DnsServer";

/**
 *  @brief IP address of the default gateway when not using DHCP.
 */
std::atomic<uint32_t> WiFiRequestHandler::_defaultGateway(0);

/**
 *  @brief Name of the storage space holding the IP address of the default gateway.
 */
const char *WiFiRequestHandler::DEFAULT_GATEWAY_NAME = "Gateway";

/**
 *  @brief Board (station) MAC address requested through SetMacAddress, applied the next
 *      time the station interface starts.  Only meaningful when
 *      _stationMacAddressOverrideIsSet is true.
 */
uint8_t WiFiRequestHandler::_stationMacAddressOverride[6] = {};

/**
 *  @brief True when SetMacAddress has been given a board MAC address to apply.
 */
bool WiFiRequestHandler::_stationMacAddressOverrideIsSet = false;

/**
 *  @brief Software access point MAC address requested through SetMacAddress, applied the
 *      next time the access point interface starts.  Only meaningful when
 *      _softApMacAddressOverrideIsSet is true.
 */
uint8_t WiFiRequestHandler::_softApMacAddressOverride[6] = {};

/**
 *  @brief True when SetMacAddress has been given a software access point MAC address to
 *      apply.
 */
bool WiFiRequestHandler::_softApMacAddressOverrideIsSet = false;

/**
 *  @brief Default value for the WiFi timeout.
 */
std::atomic<uint32_t> WiFiRequestHandler::_wifiTimeout(30000);

/**
 *  @brief Name of the storage space holding the default WiFi timeout.
 */
const char *WiFiRequestHandler::DEFAULT_WIFI_TIMEOUT_NAME = "WiFiTimeout";

/**
 * @brief Empty string used when SSID and password are not available.
 */
char WiFiRequestHandler::_emptyString[1] = {0};

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
        throw MultipleInstancesException();
    }
    else
    {
        _instance = this;
    }
    //
    //  Get configuration from NVS.
    //
    {
        //
        //  GetConfigurationValue returns a copy the caller owns, so a value already held is
        //  replaced, and released, only when NVS actually holds one.  _emptyString is not on
        //  the heap and is never released.
        //
        MutexLock lock(CredentialsMutex());
        char *storedAccessPoint = const_cast<char *>(SystemRequestHandler::GetConfigurationValue(DEFAULT_ACCESS_POINT_NAME, nullptr));
        if (storedAccessPoint != nullptr)
        {
            if ((_defaultAccessPoint != nullptr) && (_defaultAccessPoint != WiFiRequestHandler::_emptyString))
            {
                free(_defaultAccessPoint);
            }
            _defaultAccessPoint = storedAccessPoint;
        }
        char *storedPassword = const_cast<char *>(SystemRequestHandler::GetConfigurationValue(PASSWORD_NAME, nullptr));
        if (storedPassword != nullptr)
        {
            if ((_password != nullptr) && (_password != WiFiRequestHandler::_emptyString))
            {
                free(_password);
            }
            _password = storedPassword;
        }
    }
    _stationNetIfHandle = nullptr;
    _softApNetIfHandle = nullptr;
    {
        //
        //  GetConfigurationValue returns a copy on the heap the caller owns, of the stored
        //  code or of the default.  Only the matching entry of _validCountryCodes is kept, so
        //  the copy is released here rather than held.
        //
        const char *stored = SystemRequestHandler::GetConfigurationValue(COUNTRY_CODE_NAME, _validCountryCodes[0]);
        const char *countryCode = FindCountryCode(stored);
        if (countryCode == nullptr)
        {
            //
            //  NVS held a country code that predates _validCountryCodes, or was corrupted, or
            //  written by another code path.  Fall back to the default rather than carrying an
            //  unvalidated value into esp_wifi_set_country_code().
            //
            ERROR_MESSAGE("WiFiRequestHandler: NVS held an invalid country code (%s); using the default.", (stored == nullptr) ? "<null>" : stored);
            countryCode = _validCountryCodes[0];
        }
        Utils::DeleteAndNullifyPointer(stored);
        _countryCode = countryCode;
        TRACE_MESSAGE("WiFiRequestHandler: Country code is %s", countryCode);
    }
    _maximumRetryCount = SystemRequestHandler::GetConfigurationValue(MAXIMUM_RETRY_COUNT_NAME, static_cast<uint32_t>(DEFAULT_MAXIMUM_RETRY_COUNT));
    TRACE_MESSAGE("WiFiRequestHandler: Maximum retry count is %" PRIu32, _maximumRetryCount.load());
    //
    //  Setup the timezones ready for when we get the time from a timeserver.
    //
    setenv("TZ", _timezone, 1);
    tzset();
    //
    //  Set up the structures need to deal with poll() requests in an asynchronous manner.
    //
    _pollRequestListMutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the poll request list mutex");
    _socketHandlesMutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the socket handle table mutex");
    //
    //  Get ready for nodes connecting when configured as an access point.
    //
    _connectedNodesListMutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the connected nodes list mutex");
    _connectedNodes.clear();

    //
    //  Finally set some default values for private fields.
    //
    ClearIpAndDisconnectData();
    {
        MutexLock lock(EventStateMutex());
        memset(&_apInformation, 0, sizeof(wifi_event_sta_connected_t));
    }
    _progressiveDelay = new ProgressiveDelay();
    WiFiRequestHandler::_emptyString[0] = 0;
}

/**
 *  @brief Create a new object and set the message dispatcher accordingly.
 *
 *  @param messageDispatcher
 *      Message Dispatcher object that can be used to send messages to
 *      the STM32.
 */
WiFiRequestHandler::WiFiRequestHandler(IMessageDispatcher *messageDispatcher) : WiFiRequestHandler()
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
    //
    //  Production creates this object once and never destroys it, so this only matters to
    //  the tests.  The constructor throws when an instance already exists and nothing
    //  catches that, so a handler that is discarded has to hand the pointer back or the
    //  next construction aborts the board.
    //
    _instance = nullptr;
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
    const uint32_t maximumRetryCount = _maximumRetryCount.load();
    return ((maximumRetryCount == 0) ? 3 : maximumRetryCount);
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
    TRACE_MESSAGE("WiFiRequestHandler::SetMaximumRetryCount: %" PRIu32, value);

    StatusCodes::StatusCodes result = StatusCodes::Failure;

    if (value == _maximumRetryCount)
    {
        result = StatusCodes::CompletedOk;
    }
    else
    {
        if (SystemRequestHandler::SetConfigurationValue(MAXIMUM_RETRY_COUNT_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            _maximumRetryCount = SystemRequestHandler::GetConfigurationValue(MAXIMUM_RETRY_COUNT_NAME, static_cast<uint32_t>(DEFAULT_MAXIMUM_RETRY_COUNT));
            result = StatusCodes::CompletedOk;
        }
    }

    return (result);
}

/**
 *  @brief Get the automatic reconnect value.
 */
bool WiFiRequestHandler::GetAutomaticReconnect()
{
    return (_automaticReconnect);
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
    TRACE_MESSAGE("WiFiRequestHandler::SetAutomaticReconnect: %" PRIi32, value ? 1 : 0);
    _automaticReconnect = value;
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Get the automatically start network value.
 */
bool WiFiRequestHandler::GetAutomaticallyStartNetwork()
{
    return (_automaticallyStartNetwork);
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
    TRACE_MESSAGE("WiFiRequestHandler::SetAutomaticallyStartNetwork: %" PRIi32, value ? 1 : 0);
    _automaticallyStartNetwork = value;
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Get the use DHCP value.
 */
bool WiFiRequestHandler::GetUseDhcp()
{
    return (_useDhcp);
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
    TRACE_MESSAGE("WiFiRequestHandler::SetUseDhcp: %" PRIi32, value ? 1 : 0);
    _useDhcp = value;
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Get a copy of the default access point.
 *
 *  @returns
 *      The default access point, empty when none is configured.
 */
std::string WiFiRequestHandler::GetDefaultAccessPoint()
{
    MutexLock lock(CredentialsMutex());
    return ((_defaultAccessPoint == nullptr) ? std::string() : std::string(_defaultAccessPoint));
}

/**
 *  @brief Set default access point.
 *
 *  @param value
 *      New default access point value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::EspOutOfMemory if the value cannot be
 *      copied, StatusCodes::Failure if there is another problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetDefaultAccessPoint(const char *value)
{
    //
    //  Held for the comparison, the NVS write and the swap, so no reader copies the old
    //  string while it is being released.
    //
    MutexLock lock(CredentialsMutex());
    TRACE_MESSAGE("WiFiRequestHandler::SetDefaultAccessPoint %s", (value == nullptr) ? "NULL" : value);
    StatusCodes::StatusCodes result = StatusCodes::Failure;

    if (value == nullptr)
    {
        value = WiFiRequestHandler::_emptyString;
    }
    if (!_defaultAccessPoint)
    {
        _defaultAccessPoint = WiFiRequestHandler::_emptyString;
    }

    if (strcmp(value, _defaultAccessPoint) == 0)
    {
        result = StatusCodes::CompletedOk;
    }
    else
    {
        //
        //  Copied before NVS is written: if memory runs out, neither NVS nor the value held
        //  here changes, so the two cannot disagree.
        //
        char *copy = strdup(value);
        if (copy == NULL)
        {
            result = StatusCodes::EspOutOfMemory;
        }
        else
        {
            if (SystemRequestHandler::SetConfigurationValue(DEFAULT_ACCESS_POINT_NAME, value) == NvsManager::ErrorCodes::Ok)
            {
                //
                //  _emptyString is not on the heap and must not be released.
                //
                if (_defaultAccessPoint != WiFiRequestHandler::_emptyString)
                {
                    free(_defaultAccessPoint);
                }
                _defaultAccessPoint = copy;
                result = StatusCodes::CompletedOk;
            }
            else
            {
                free(copy);
            }
        }
    }
    return (result);
}

/**
 *  @brief Get a copy of the password for the default access point.
 *
 *  @returns
 *      The password, empty when none is configured.
 */
std::string WiFiRequestHandler::GetPassword()
{
    MutexLock lock(CredentialsMutex());
    return ((_password == nullptr) ? std::string() : std::string(_password));
}

/**
 *  @brief Set password for the default access point.
 *
 *  @param value
 *      New password value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::EspOutOfMemory if the value cannot be
 *      copied, StatusCodes::Failure if there is another problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetPassword(const char *value)
{
    //
    //  Held for the comparison, the NVS write and the swap, so no reader copies the old
    //  string while it is being released.
    //
    MutexLock lock(CredentialsMutex());
    TRACE_MESSAGE("WiFiRequestHandler::SetPassword %s", (value == nullptr) ? "NULL" : value);
    StatusCodes::StatusCodes result = StatusCodes::Failure;

    if (value == nullptr)
    {
        value = WiFiRequestHandler::_emptyString;
    }
    if (!_password)
    {
        _password = WiFiRequestHandler::_emptyString;
    }

    if (strcmp(value, _password) == 0)
    {
        result = StatusCodes::CompletedOk;
    }
    else
    {
        //
        //  Copied before NVS is written: if memory runs out, neither NVS nor the value held
        //  here changes, so the two cannot disagree.
        //
        char *copy = strdup(value);
        if (copy == NULL)
        {
            result = StatusCodes::EspOutOfMemory;
        }
        else
        {
            if (SystemRequestHandler::SetConfigurationValue(PASSWORD_NAME, value) == NvsManager::ErrorCodes::Ok)
            {
                //
                //  _emptyString is not on the heap and must not be released.
                //
                if (_password != WiFiRequestHandler::_emptyString)
                {
                    free(_password);
                }
                _password = copy;
                result = StatusCodes::CompletedOk;
            }
            else
            {
                free(copy);
            }
        }
    }
    return (result);
}

/**
 *  @brief Get the country code for the WiFi configuration.
 */
const char *WiFiRequestHandler::GetCountryCode()
{
    return (WiFiRequestHandler::_countryCode.load());
}

/**
 *  @brief Indicate whether the given country code is one of _validCountryCodes.
 *
 *  @param code
 *      Country code to check, which may be nullptr.
 *
 *  @returns
 *      true if code is non-null and matches one of _validCountryCodes.
 */
bool WiFiRequestHandler::IsValidCountryCode(const char *code)
{
    return (FindCountryCode(code) != nullptr);
}

/**
 *  @brief Find the entry of _validCountryCodes matching the given country code.
 *
 *  @param code
 *      Country code to look up, which may be nullptr.
 *
 *  @returns
 *      The matching entry of _validCountryCodes, which is never released, or nullptr if code
 *      is not one of them.
 */
const char *WiFiRequestHandler::FindCountryCode(const char *code)
{
    const char *result = nullptr;

    if (code != nullptr)
    {
        for (uint32_t countryCodeIndex = 0; countryCodeIndex < (sizeof(WiFiRequestHandler::_validCountryCodes) / sizeof(WiFiRequestHandler::_validCountryCodes[0])); countryCodeIndex++)
        {
            if (!strcmp(code, WiFiRequestHandler::_validCountryCodes[countryCodeIndex]))
            {
                result = WiFiRequestHandler::_validCountryCodes[countryCodeIndex];
                break;
            }
        }
    }
    return (result);
}

/**
 *  @brief Set the country code for the WiFi configuration.
 *
 *  @param value
 *      New country code value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetCountryCode(const char *value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetCountryCode %s", (value == nullptr) ? "NULL" : value);
    StatusCodes::StatusCodes result = StatusCodes::InvalidCountryCode;

    //
    //  The code kept is the matching entry of _validCountryCodes, which is never released,
    //  so a reader on another task can never be left holding a freed string (issue #168).
    //
    const char *countryCode = FindCountryCode(value);
    if ((countryCode != nullptr) && (countryCode == _countryCode.load()))
    {
        result = StatusCodes::CompletedOk;
    }
    else
    {
        if ((countryCode != nullptr) && (SystemRequestHandler::SetConfigurationValue(COUNTRY_CODE_NAME, countryCode) == NvsManager::ErrorCodes::Ok))
        {
            _countryCode = countryCode;
            result = StatusCodes::CompletedOk;
        }
    }
    return (result);
}

/**
 *  @brief Get the static IP address value.
 */
uint32_t WiFiRequestHandler::GetIpAddress()
{
    return (_staticIpAddress);
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
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Get the subnet mask when static addressing is used..
 */
uint32_t WiFiRequestHandler::GetSubNetMask()
{
    return (_staticSubNetMask);
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
StatusCodes::StatusCodes WiFiRequestHandler::SetSubNetMask(uint32_t value)
{
    _staticSubNetMask = value;
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Get the static DNS server value.
 */
uint32_t WiFiRequestHandler::GetDnsServer()
{
    return (_dnsServer);
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
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Get the static default gateway value.
 */
uint32_t WiFiRequestHandler::GetDefaultGateway()
{
    return (_defaultGateway);
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
    return (StatusCodes::CompletedOk);
}

/**
 *  @brief Get the MAC address of the board.
 *
 *  @note esp_wifi_get_mac() reports the address the driver is actually using, which
 *      reflects an address SetMacAddress() has applied, but only works once the WiFi
 *      driver is initialised.  esp_read_mac() reads the factory address straight from
 *      eFuse and works at any time, so it is tried first before the driver is up and used
 *      as a fallback afterwards, in the unexpected case esp_wifi_get_mac() fails.
 *
 *  @param macAddress
 *      Pointer to the buffer to hold the MAC address.
 *
 *  @param interface
 *      ESP interface to get the MAC address.  Valid values:
 *          WIFI_IF_STA     Board MAC address
 *          WIFI_IF_AP      Software access point MAC address
 */
void WiFiRequestHandler::GetMacAddress(uint8_t * const macAddress, wifi_interface_t interface)
{
    if (interface == WIFI_IF_STA)
    {
        if (esp_wifi_get_mac(WIFI_IF_STA, macAddress) != ESP_OK)
        {
            if (esp_read_mac(macAddress, ESP_MAC_WIFI_STA) != ESP_OK)
            {
                memset(macAddress, 0, 6);
            }
        }
    }
    else
    {
        if (esp_wifi_get_mac(WIFI_IF_AP, macAddress) != ESP_OK)
        {
            memset(macAddress, 0, 6);
        }
    }
}

/**
 *  @brief Mutex guarding _stationMacAddressOverride and _stationMacAddressOverrideIsSet.
 *
 *  A function-local static is created exactly once, on first use, even when several tasks
 *  get here at the same time.
 *
 *  @returns
 *      The station MAC address override mutex.
 */
SemaphoreHandle_t WiFiRequestHandler::StationMacAddressOverrideMutex()
{
    static const SemaphoreHandle_t mutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the station MAC address override mutex");
    return (mutex);
}

/**
 *  @brief Set the MAC address of the board.
 *
 *  @note esp_wifi_set_mac() can only be called once the WiFi driver is initialised and the
 *      target interface is stopped, neither of which is true until a connection is
 *      attempted.  The requested address is therefore stored here and applied by
 *      StartWiFiInterface() the next time the interface (re)starts.
 *
 *  @param macAddress
 *      New MAC address, 6 bytes.  Must be a unicast address, so the least-significant bit
 *      of the first byte must be clear.
 *
 *  @param interface
 *      Interface the MAC address refers to.  Valid values:
 *          WIFI_IF_STA     Board MAC address
 *          WIFI_IF_AP      Software access point MAC address
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetMacAddress(const uint8_t * const macAddress, wifi_interface_t interface)
{
    StatusCodes::StatusCodes result = StatusCodes::Failure;

    if ((macAddress != nullptr) && ((macAddress[0] & 0x01) == 0))
    {
        if (interface == WIFI_IF_STA)
        {
            MutexLock lock(StationMacAddressOverrideMutex());
            memcpy(_stationMacAddressOverride, macAddress, sizeof(_stationMacAddressOverride));
            _stationMacAddressOverrideIsSet = true;
        }
        else
        {
            memcpy(_softApMacAddressOverride, macAddress, sizeof(_softApMacAddressOverride));
            _softApMacAddressOverrideIsSet = true;
        }
        result = StatusCodes::CompletedOk;
    }
    return (result);
}

/**
 *  @brief Get the currently selected antenna.
 *
 *  @return
 *      Current Antenna in use as an unsigned 8-bit integer.
 */
uint8_t WiFiRequestHandler::GetAntenna()
{
    return (SystemRequestHandler::GetConfigurationValue(ANTENNA_TYPE_NAME, static_cast<uint8_t>(AntennaTypes::OnBoard)));
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

    return (result);
}

/**
 *  @brief Get the WiFi timeout value (milliseconds).
 */
uint32_t WiFiRequestHandler::GetWiFiTimeout()
{
    return (_wifiTimeout);
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
    return (StatusCodes::CompletedOk);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Get a pointer to this instance of the WiFiRequest Handler object.
 *
 *  Note: There should be only one instance of this object in the system.
 *
 * @return Pointer the this instance of the WiFiRequest handler.
 */
WiFiRequestHandler *WiFiRequestHandler::GetInstance()
{
    return (_instance);
}

/**
 *  @brief Mutex guarding _apInformation, _disconnectData and _ipInformation.
 *
 *  A function-local static is created exactly once, on first use, even when several tasks
 *  get here at the same time.
 *
 *  @returns
 *      The event state mutex.
 */
SemaphoreHandle_t WiFiRequestHandler::EventStateMutex()
{
    static const SemaphoreHandle_t mutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the WiFi event state mutex");
    return (mutex);
}

/**
 *  @brief Get a copy of the data from the most recent disconnection.
 *
 *  @returns
 *      Copy of _disconnectData, taken under EventStateMutex().
 */
wifi_event_sta_disconnected_t WiFiRequestHandler::GetDisconnectData()
{
    MutexLock lock(EventStateMutex());
    return (_disconnectData);
}

/**
 *  @brief Clear the IP and disconnect data.
 */
void WiFiRequestHandler::ClearIpAndDisconnectData()
{
    if (_instance != nullptr)
    {
        MutexLock lock(EventStateMutex());
        memset(&_instance->_disconnectData, 0, sizeof(_disconnectData));
        memset(&_instance->_ipInformation, 0, sizeof(_ipInformation));
    }
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

    TRACE_MESSAGE("Setting time to %" PRIi32, static_cast<int32_t>(tv->tv_sec));

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
    return ((antenna == AntennaTypes::OnBoard) ? WiFiRequestHandler::ON_BOARD_ANTENNA_PIN : WiFiRequestHandler::EXTERNAL_ANTENNA_PIN);
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
    if (newAntennaPin == oldAntennaPin)
    {
        TRACE_MESSAGE("Antenna not changed");
    }
    else
    {
        //
        //  Make sure the old antenna is switched off before the new one is turned on.
        //
        TRACE_MESSAGE("SwitchAntenna: Switching from antenna %" PRIu32 " to %" PRIu32, oldAntennaPin, newAntennaPin);
        gpio_set_level(oldAntennaPin, 0);
        gpio_set_level(newAntennaPin, 1);
        SystemRequestHandler::SetConfigurationValue(ANTENNA_TYPE_NAME, static_cast<uint8_t>(newAntenna));
    }
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
    _xWiFiEventGroup = Utils::RequireCreated(xEventGroupCreate(), "the WiFi event group");
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_ALL_BITS);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
    Utils::RequireTaskCreated(xTaskCreate(Task, COMPONENT_NAME, TASK_STACK_SIZE, this, PRIORITY_WIFI_TASK, &_taskHandle), "the WiFi task");
    TRACE_MESSAGE("Setup: Exit");
}

/**
 *  @brief Copy the IP and access point structure into the connection event data structure.
 *
 *  @param ed
 *    Pointer to the connection event data structure to be populated.
 */
void WiFiRequestHandler::PopulateConnectionEventDataStructure(Esp32Messaging::ConnectEventData * const ed)
{
    TRACE_MESSAGE("PopulateConnectionEventDataStructure: Enter");

    *ed = {};
    //
    //  Copied under the lock, so the IP and access point details all come from the same
    //  events rather than some from an event that arrives part way through (issue #287).
    //
    esp_netif_ip_info_t ipInformation;
    wifi_event_sta_connected_t apInformation;
    {
        MutexLock lock(EventStateMutex());
        ipInformation = _ipInformation;
        apInformation = _apInformation;
    }
    //
    //  IP information.
    //
    memcpy(&ed->IpAddress, &ipInformation.ip.addr, sizeof(ed->IpAddress));
    memcpy(&ed->SubnetMask, &ipInformation.netmask.addr, sizeof(ed->SubnetMask));
    memcpy(&ed->Gateway, &ipInformation.gw.addr, sizeof(ed->Gateway));
    //
    //  Access point information.
    //
    memcpy(&ed->Ssid, apInformation.ssid, apInformation.ssid_len);
    memcpy(&ed->Bssid, apInformation.bssid, sizeof(ed->Bssid));
    ed->Channel = apInformation.channel;
    Mapping::GetStmValue(Mapping::NetworkAuthenticationType, apInformation.authmode, reinterpret_cast<int8_t *>(&ed->AuthenticationMode));

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

    Esp32Messaging::ConnectEventData *ed = static_cast<Esp32Messaging::ConnectEventData *>(pvPortMalloc(sizeof(Esp32Messaging::ConnectEventData)));
    if (ed != nullptr)
    {
        PopulateConnectionEventDataStructure(ed);
        uint32_t payloadLength = Encoders::EncodedConnectEventDataBufferSize(ed);
        uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        if (payload != nullptr)
        {
            Encoders::EncodeConnectEventData(ed, payload);
            RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::NetworkConnectedEvent, status, payload, payloadLength);
        }
        else
        {
            ERROR_MESSAGE("Failed to allocate memory for ConnectEventData payload.");
        }
        Utils::DeleteAndNullifyPointer(ed);
    }
    else
    {
        ERROR_MESSAGE("Failed to allocate memory for ConnectEventData");
    }

    TRACE_MESSAGE("RaiseConnectToAccessPointEvent: Exit");
}

/**
 *  @brief Raise the DisconnectEvent for the managed code.
 *
 */
void WiFiRequestHandler::RaiseDisconnectEvent()
{
    TRACE_MESSAGE("RaiseDisconnectEvent: Enter");

    Esp32Messaging::DisconnectEventData *ed = static_cast<Esp32Messaging::DisconnectEventData *>(pvPortMalloc(sizeof(Esp32Messaging::DisconnectEventData)));

    if (ed != nullptr)
    {
        memset(ed, 0, sizeof(Esp32Messaging::DisconnectEventData));

        //
        //  A copy taken under the lock, so the ssid, its length and the reason all come from
        //  the same disconnection (issue #287).
        //
        const wifi_event_sta_disconnected_t disconnectData = GetDisconnectData();

        // Station information
        //
        //  disconnectData.ssid is 32 bytes, one shorter than ed->Ssid, so the copy length
        //  must come from the source, not sizeof(ed->Ssid); the memset above already
        //  NUL-terminates ed->Ssid.
        //
        memcpy(&ed->Ssid, &disconnectData.ssid, sizeof(disconnectData.ssid));
        memcpy(&ed->SsidLength, &disconnectData.ssid_len, sizeof(ed->SsidLength));
        memcpy(&ed->Bssid, &disconnectData.bssid, sizeof(ed->Bssid));
        memcpy(&ed->Reason, &disconnectData.reason, sizeof(ed->Reason));

        uint32_t payloadLength = Encoders::EncodeDisconnectEventDataBufferSize(ed);
        uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        if (payload != nullptr)
        {
            Encoders::EncodeDisconnectEventData(ed, payload);
            RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::NetworkDisconnectedEvent, StatusCodes::CompletedOk, payload, payloadLength);
        }
        else
        {
            ERROR_MESSAGE("Failed to allocate memory for DisconnectEventData payload. ");
        }
        Utils::DeleteAndNullifyPointer(ed);
    }

    TRACE_MESSAGE("RaiseDisconnectEvent: Exit");
}

/**
 * @brief Raise the NetworkConnectingEvent for the managed code.
 */
void WiFiRequestHandler::RaiseNetworkConnectingEvent()
{
    TRACE_MESSAGE("RaiseNetworkConnectingEvent: Enter");

    SendIntegerResult(Esp32Interfaces::WiFi, WiFiFunction::NetworkConnectingEvent, StatusCodes::CompletedOk, _currentRetryCount.load());

    TRACE_MESSAGE("RaiseNetworkConnectingEvent: Exit");
}

/**
 * @brief Raise the NetworkRetryCountExceededEvent for the managed code.
 */
void WiFiRequestHandler::RaiseNetworkRetryCountExceededEvent()
{
    TRACE_MESSAGE("RaiseNetworkRetryCountExceededEvent: Enter");

    SendIntegerResult(Esp32Interfaces::WiFi, WiFiFunction::NetworkConnectionRetryCountExceededEvent, StatusCodes::CompletedOk, _currentRetryCount.load());

    TRACE_MESSAGE("RaiseNetworkRetryCountExceededEvent: Exit");
}

/**
 * @brief Raise an error event for the WiFi interface.
 *
 * @param statusCode Status code
 *
 * @param payload Heap-allocated payload for the error event, or nullptr.  Ownership passes
 *        to this method: it is either handed to the outgoing message or released here, on
 *        every path, before the method returns.
 *
 * @param payloadLength Length of the payload.
 */
void WiFiRequestHandler::RaiseErrorEvent(StatusCodes::StatusCodes statusCode, uint8_t *payload, uint32_t payloadLength)
{
    TRACE_MESSAGE("RaiseErrorEvent: Enter");

    if (payload == nullptr)
    {
        payloadLength = 0;
    }
    else
    {
        if (payloadLength == 0)
        {
            Utils::DeleteAndNullifyPointer(payload);
        }
    }

    Message *message = Message::CreateOnHeap(MessageTypes::Event, Esp32Interfaces::WiFi, WiFiFunction::ErrorEvent, statusCode, _messageDispatcher->GetNextMessageID(), payload, payloadLength);
    if (message == nullptr)
    {
        //
        //  CreateOnHeap takes ownership of the payload so it has to be released here as the sender assumes
        //  that the message has been created and the queue processing will take care of releasing the payload.
        //
        ERROR_MESSAGE("RaiseErrorEvent: Cannot create the message for the error event.");
        Utils::DeleteAndNullifyPointer(payload);
    }
    else
    {
        _messageDispatcher->QueueMessageForStm32(message);
    }

    TRACE_MESSAGE("RaiseErrorEvent: Exit");
}

/**
 * @brief Raise the NetworkRaiseNetworkGotIpEvent for the managed code.
 */
void WiFiRequestHandler::RaiseNetworkGotIpEvent()
{
    TRACE_MESSAGE("RaiseNetworkGotIpEvent: Enter");

    esp_netif_dns_info_t dnsInformation = {};
    esp_err_t dnsResult = esp_netif_get_dns_info(_stationNetIfHandle, ESP_NETIF_DNS_MAIN, &dnsInformation);
    if (dnsResult != ESP_OK)
    {
        ERROR_MESSAGE("Failed to get DNS information for the station network interface.");
    }

    Esp32Messaging::GotIpEventData *ed = static_cast<Esp32Messaging::GotIpEventData *>(pvPortMalloc(sizeof(Esp32Messaging::GotIpEventData)));
    if (ed != nullptr)
    {
        memset(ed, 0, sizeof(Esp32Messaging::GotIpEventData));
        if (dnsResult == ESP_OK)
        {
            memcpy(&ed->DnsAddress, &dnsInformation.ip.u_addr.ip4.addr, sizeof(ed->DnsAddress));
        }
        uint32_t payloadLength = Encoders::EncodedGotIpEventDataBufferSize(ed);
        uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        if (payload != nullptr)
        {
            Encoders::EncodeGotIpEventData(ed, payload);
            RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::NetworkGotIpEvent, StatusCodes::CompletedOk, payload, payloadLength);
        }
        else
        {
            ERROR_MESSAGE("Failed to allocate memory for GotIpDataEvent payload. ");
        }
        Utils::DeleteAndNullifyPointer(ed);
    }
    TRACE_MESSAGE("RaiseNetworkGotIpEvent: Exit");
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
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_STOP:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_STOP");
                StationStoppedEvent();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_CONNECTED");
                StationConnectedEvent(static_cast<wifi_event_sta_connected_t *>(eventData));
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_DISCONNECTED");
                StationDisconnectedEvent(static_cast<wifi_event_sta_disconnected_t *>(eventData));
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
                NodeDisconnectedEvent(static_cast<wifi_event_ap_stadisconnected_t *>(eventData));
                break;
            case WIFI_EVENT_AP_PROBEREQRECVED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_PROBEREQRECVED");
                break;
            case WIFI_EVENT_HOME_CHANNEL_CHANGE:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_HOME_CHANNEL_CHANGE");
                break;
            default:
                TRACE_MESSAGE("EventHandlerHelper: Unhandled WiFi Event ID %" PRIi32, eventId);
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
                    StationGotIpEvent(static_cast<ip_event_got_ip_t *>(eventData));
                    break;
                case IP_EVENT_STA_LOST_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_STA_LOST_IP");
                    break;
                case IP_EVENT_AP_STAIPASSIGNED:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_AP_STAIPASSIGNED");
                    NodeConnectedEvent(static_cast<ip_event_ap_staipassigned_t *>(eventData));
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
                    TRACE_MESSAGE("EventHandlerHelper: Unhandled IP Event ID %" PRIi32, eventId);
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
    try
    {
        switch (request->Function)
        {
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
                ScanForAccessPoints(request);
                break;
            case WiFiFunction::ClearDefaultAccessPoint:
                ClearDefaultAccessPointAndPassword(request);
                break;
            case WiFiFunction::StartAccessPoint:
                StartAccessPoint(request);
                break;
            case WiFiFunction::StopAccessPoint:
                StopAccessPoint(request);
                break;
            case WiFiFunction::SetAntenna:
                SetAntennaRequest(request);
                break;
            case WiFiFunction::Close:
                Close(request);
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::SetSockOpt:
                SystemRequestHandler::SharedThreadPool()->Execute(SetSockOpt, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::GetSockOpt:
                SystemRequestHandler::SharedThreadPool()->Execute(GetSockOpt, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::FreeAddrInfo:
                SystemRequestHandler::SharedThreadPool()->Execute(FreeAddrInfo, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::Connect:
                SystemRequestHandler::SharedThreadPool()->Execute(Connect, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::Socket:
                SystemRequestHandler::SharedThreadPool()->Execute(Socket, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::GetAddrInfo:
                SystemRequestHandler::SharedThreadPool()->Execute(GetAddrInfo, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::SendTo:
                SystemRequestHandler::SharedThreadPool()->Execute(SendTo, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::RecvFrom:
                SystemRequestHandler::SharedThreadPool()->Execute(RecvFrom, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::Poll:
                Poll(request);
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::Send:
                SystemRequestHandler::SharedThreadPool()->Execute(Send, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::Accept:
                SystemRequestHandler::SharedThreadPool()->Execute(Accept, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::Bind:
                SystemRequestHandler::SharedThreadPool()->Execute(Bind, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::Listen:
                SystemRequestHandler::SharedThreadPool()->Execute(Listen, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::Ioctl:
                SystemRequestHandler::SharedThreadPool()->Execute(Ioctl, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::GetSockName:
                SystemRequestHandler::SharedThreadPool()->Execute(GetSockName, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::GetPeerName:
                SystemRequestHandler::SharedThreadPool()->Execute(GetPeerName, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::SendMsg:
                SystemRequestHandler::SharedThreadPool()->Execute(SendMsg, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            case WiFiFunction::RecvMsg:
                SystemRequestHandler::SharedThreadPool()->Execute(RecvMsg, static_cast<void *>(request));
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
            default:
                TRACE_MESSAGE("DispatchRequest: Unknown WiFi message received, function %" PRIu32, request->Function);
                if ((request->Payload != nullptr) && (request->PayloadLength > 0))
                {
                    TRACE_HEX_BUFFER(request->Payload, request->PayloadLength);
                }
                RespondToUnknownFunction(request);
                //
                //  No handler runs for this request, so nothing else will set the bit and the
                //  wait below would block the WiFi task for good.
                //
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
                break;
        }
    }
    catch (const ThreadPoolException &e)
    {
        TRACE_MESSAGE("DispatchRequest: Exception caught: %s", e.what());
        request->DeletePayload();
        request->MessageType = MessageTypes::Response;
        request->StatusCode = StatusCodes::ThreadPoolIsFull;
        _messageDispatcher->QueueMessageForStm32(request);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
    }
    xEventGroupWaitBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    TRACE_MESSAGE("DispatchRequest: Exit");
}

/**
 * @brief Prepare the WiFi interface for deep sleep.
 */
StatusCodes::StatusCodes WiFiRequestHandler::PrepareForDeepSleep()
{
    TRACE_MESSAGE("PrepareForDeepSleep(): Enter");

    WiFiRequestHandler *wifiHandler = WiFiRequestHandler::GetInstance();
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (wifiHandler != nullptr)
    {
        result = wifiHandler->StopWiFIAdapter();
    }

    TRACE_MESSAGE("PrepareForDeepSleep(): Exit");

    return (result);
}

/**
 *  @brief Stop the WiFi adapter.
 */
StatusCodes::StatusCodes WiFiRequestHandler::StopWiFIAdapter()
{
    TRACE_MESSAGE("StopWiFIAdapter(): Enter");

    esp_err_t result = ESP_OK;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (bits & WIFI_CONNECTED_BIT)
    {
        result = esp_wifi_disconnect();
    }

    if (result == ESP_OK)
    {
        result = esp_wifi_stop();
    }

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

    TRACE_MESSAGE("StopWiFIAdapter(): Exit, ESP error code %" PRIi32 " (%s)", result, esp_err_to_name(result));
    return (Mapping::GetStatusCode(result));
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
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);
    }
    else
    {
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
        sc = StatusCodes::Failure;
    }
    ClearIpAndDisconnectData();

    TRACE_MESSAGE("DisconnectFromAccessPoint(): Exit");
    return (sc);
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

    StatusCodes::StatusCodes sc = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        //
        //  The bit is only set when there is a connection to drop.  It is cleared by
        //  StationDisconnectedEvent, which is only raised when a disconnection actually
        //  happens, so setting it with nothing connected leaves it set for good.  The
        //  next connection attempt to fail would then be taken for a requested
        //  disconnection and would not be retried.
        //
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_DISCONNECT_REQUESTED_BIT);
        sc = DisconnectFromAccessPoint();
    }
    if (sc == StatusCodes::CompletedOk)
    {
        Esp32Messaging::DisconnectFromAccessPointRequest *request = Encoders::ExtractDisconnectFromAccessPointRequest(message->Payload);

        if (request->TurnOffWiFiInterface == 1)
        {
            sc = StopWiFIAdapter();
        }
        Utils::DeleteAndNullifyPointer(request);
    }

    message->DeletePayload();
    message->StatusCode = sc;
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

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
    message->DeletePayload();

    TRACE_MESSAGE("Switching antenna to %" PRIu8 " with persist set to %" PRIu8, request->Antenna, request->Persist);

    if (request->Persist)
    {
        SetAntenna(request->Antenna); // This method also switches the antenna.
    }
    else
    {
        SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(request->Antenna));
    }

    Utils::DeleteAndNullifyPointer(request);
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

    return ((bits & WIFI_CONNECTED_BIT) == WIFI_CONNECTED_BIT);
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
StatusCodes::StatusCodes WiFiRequestHandler::ConnectToAccessPoint(const Esp32Messaging::AccessPointInformation *accessPointInformation)
{
    RaiseNetworkConnectingEvent();

    char *password = accessPointInformation->Password;

    TRACE_MESSAGE("IP information, IP: 0x%08" PRIx32 ", subnet: 0x%08" PRIx32 ", gateway 0x%08" PRIx32, accessPointInformation->IpAddress, accessPointInformation->SubnetMask, accessPointInformation->Gateway);

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (bits & WIFI_HAVE_IP_BIT)
    {
        return (StatusCodes::WiFiAlreadyStarted);
    }

    if ((accessPointInformation->NetworkName == nullptr) || (strlen(accessPointInformation->NetworkName) == 0))
    {
        return (StatusCodes::InvalidWiFiCredentials);
    }

    _currentRetryCount = 0;
    ClearIpAndDisconnectData();
    if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    {
        if (StartWiFiInterface(WIFI_MODE_STA, accessPointInformation) != ESP_OK)
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
        char zeroLengthString[1] = {0};
        if (password == nullptr)
        {
            password = zeroLengthString;
        }
        //
        //  A password of exactly MAXIMUM_PASSWORD_LENGTH (64) characters is valid and fills
        //  wifi_config.sta.password with no terminator, which is what the stack expects.  It
        //  copies the field into a NUL-terminated profile of its own, and for WPA2-PSK
        //  wpa_set_passphrase() treats a 64-character value as a 64-digit hex pre-shared key
        //  rather than a passphrase.  WPA3-SAE takes the password as it is, any length up to
        //  the field, so no tighter limit can be applied here without refusing valid networks.
        //
        if ((strlen(accessPointInformation->NetworkName) > MAXIMUM_SSID_LENGTH) || (strlen(password) > MAXIMUM_PASSWORD_LENGTH))
        {
            result = StatusCodes::InvalidWiFiCredentials;
            TRACE_MESSAGE("ConnectToAccessPoint: Invalid credentials");
        }
        else
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
            wifi_config_t wifi_config = {};
            wifi_config.sta.pmf_cfg = {.capable = true, .required = false};
            memcpy(wifi_config.sta.ssid, accessPointInformation->NetworkName, strlen(accessPointInformation->NetworkName));
            memcpy(wifi_config.sta.password, password, strlen(password));
            //
            //  A local rather than a member: StationStartedEvent runs on the event loop while
            //  this runs on the WiFi task, and a shared value could be overwritten between
            //  these calls (issue #168).
            //
            esp_err_t idfResult = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            if (idfResult == ESP_OK)
            {
                idfResult = esp_wifi_connect();
            }
            if (idfResult == ESP_OK)
            {
                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_HAVE_IP_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
                if (bits & WIFI_ERROR_BIT)
                {
                    result = StatusCodes::CannotConnectToAccessPoint;
                }
                else
                {
                    if (bits & WIFI_HAVE_IP_BIT)
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
                result = static_cast<StatusCodes::StatusCodes>(Mapping::GetStmValue(Mapping::EspErrorCodes, static_cast<int32_t>(idfResult), &found));
            }
        }
    }
    TRACE_MESSAGE("%s: result %" PRIu32, __func__, result);
    return (result);
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
    StatusCodes::StatusCodes result;

    Esp32Messaging::AccessPointInformation *accessPointInformation = Encoders::ExtractAccessPointInformation(message->Payload);
    message->DeletePayload();

    if (!accessPointInformation)
    {
        result = StatusCodes::EspOutOfMemory;
    }
    else
    {
        //
        //  Check if the IP address is non-zero, in this case the application is configured for a static IP.
        //  Static IP addresses must have a valid subnet mask and gateway address set.
        //
        bool staticIpIsValid = true;
        if (accessPointInformation->IpAddress != 0)
        {
            if ((accessPointInformation->SubnetMask == 0) || (accessPointInformation->Gateway == 0))
            {
                staticIpIsValid = false;
                result = StatusCodes::InvalidIp;
                RaiseErrorEvent(result, nullptr, 0);
            }
        }

        if (staticIpIsValid)
        {
            result = ConnectToAccessPoint(accessPointInformation);
        }
        Encoders::FreeAccessPointInformation(&accessPointInformation);

        Esp32Messaging::ConnectEventData data = {};

        if (result == StatusCodes::CompletedOk)
        {
            PopulateConnectionEventDataStructure(&data);
        }
        else
        {
            const wifi_event_sta_disconnected_t disconnectData = GetDisconnectData();
            memcpy(static_cast<void *>(data.Ssid), static_cast<const void *>(disconnectData.ssid), disconnectData.ssid_len);
            memcpy(static_cast<void *>(data.Bssid), static_cast<const void *>(disconnectData.bssid), sizeof(data.Bssid));
            TRACE_MESSAGE("Failed to connect to access point, code: %" PRIu8, disconnectData.reason);
            data.Reason = disconnectData.reason;
            ClearIpAndDisconnectData();
        }
        message->PayloadLength = Encoders::EncodedConnectEventDataBufferSize(&data);
        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
        if (message->Payload == nullptr)
        {
            message->PayloadLength = 0;
            result = StatusCodes::EspOutOfMemory;
        }
        else
        {
            Encoders::EncodeConnectEventData(&data, message->Payload);
        }
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    Logging::DumpMessage(COMPONENT_NAME, message);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("ConnectToAccessPoint: Exit");
}

/**
 *  @brief Connect to the default access point.
 *
 *  This blocks for up to GetWiFiTimeout() while the connection is attempted, the same
 *  as ConnectToAccessPoint(Message *), and queues a Response carrying the result on
 *  every path, reusing the request message.  A successful connection also raises
 *  NetworkConnectedEvent, as any other connection does, from StationConnectedEvent.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ConnectToDefaultAccessPoint(Message *message)
{
    TRACE_MESSAGE("ConnectToDefaultAccessPoint: Enter");

    message->DeletePayload();

    StatusCodes::StatusCodes result = StatusCodes::DefaultAccessPointNotConfigured;
    Esp32Messaging::ConnectEventData data = {};

    //
    //  "Not configured" is an empty string as well as nullptr: SetDefaultAccessPoint(nullptr),
    //  which ClearDefaultAccessPointAndPassword uses, stores an empty string rather than
    //  clearing the pointer, and it stays that way for the rest of the app's life.  nullptr
    //  itself is only ever seen the first time, before NVS has been read.
    //
    //
    //  Copies, held until the connection attempt is over: a configuration change on the
    //  System task can replace the stored strings at any point during it.
    //
    const std::string accessPoint = GetDefaultAccessPoint();
    const std::string password = GetPassword();
    if (!accessPoint.empty())
    {
        Esp32Messaging::AccessPointInformation accessPointInformation = {};
        accessPointInformation.NetworkName = const_cast<char *>(accessPoint.c_str());
        accessPointInformation.Password = const_cast<char *>(password.c_str());
        if (!_useDhcp)
        {
            accessPointInformation.IpAddress = GetIpAddress();
            accessPointInformation.SubnetMask = GetSubNetMask();
            accessPointInformation.Gateway = GetDefaultGateway();
        }
        result = ConnectToAccessPoint(&accessPointInformation);

        if (result == StatusCodes::CompletedOk)
        {
            PopulateConnectionEventDataStructure(&data);
        }
        else
        {
            const wifi_event_sta_disconnected_t disconnectData = GetDisconnectData();
            memcpy(static_cast<void *>(data.Ssid), static_cast<const void *>(disconnectData.ssid), disconnectData.ssid_len);
            memcpy(static_cast<void *>(data.Bssid), static_cast<const void *>(disconnectData.bssid), sizeof(data.Bssid));
            data.Reason = disconnectData.reason;
            ClearIpAndDisconnectData();
        }
    }
    else
    {
        //
        //  No connection was attempted, so there is no StationConnectedEvent or
        //  StationDisconnectedEvent to raise NetworkConnectedEvent for the managed code.
        //  Raise it directly instead; data stays zeroed, as there is nothing to report.
        //
        RaiseConnectToAccessPointEvent(result);
    }

    message->PayloadLength = Encoders::EncodedConnectEventDataBufferSize(&data);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    if (message->Payload == nullptr)
    {
        message->PayloadLength = 0;
        result = StatusCodes::EspOutOfMemory;
    }
    else
    {
        Encoders::EncodeConnectEventData(&data, message->Payload);
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    Logging::DumpMessage(COMPONENT_NAME, message);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

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
    StatusCodes::StatusCodes sc = SetDefaultAccessPoint(nullptr);
    StatusCodes::StatusCodes sc1 = SetPassword(nullptr);
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
 *  @param mode
 *      WiFi mode of the ESP32 (Station, Access Point or both).
 *
 *  @param accessPointInformation
 *      Information about the configuration of the access point to connect to or the
 *      IP information for a station.
 *
 *  @return
 *      ESP_OK if the interface is initialised correctly, an error code otherwise.
 */
esp_err_t WiFiRequestHandler::StartWiFiInterface(const wifi_mode_t mode, const Esp32Messaging::AccessPointInformation *accessPointInformation)
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
            _stationNetIfHandle = nullptr;
            _softApNetIfHandle = nullptr;
            if ((mode == WIFI_MODE_STA) || (mode == WIFI_MODE_APSTA))
            {
                _stationNetIfHandle = esp_netif_create_default_wifi_sta();
            }

            if ((mode == WIFI_MODE_AP) || (mode == WIFI_MODE_APSTA))
            {
                _softApNetIfHandle = esp_netif_create_default_wifi_ap();
            }

            if (_stationNetIfHandle == nullptr && ((mode == WIFI_MODE_STA) || (mode == WIFI_MODE_APSTA)))
            {
                result = ESP_FAIL;
            }

            if (_softApNetIfHandle == nullptr && ((mode == WIFI_MODE_AP) || (mode == WIFI_MODE_APSTA)))
            {
                result = ESP_FAIL;
            }
        }

        //
        //  Set IP address if static address has been selected otherwise DHCP will be used.
        //
        if ((result == ESP_OK) && ((accessPointInformation) && (accessPointInformation->IpAddress != 0) && (accessPointInformation->Gateway != 0) && (accessPointInformation->SubnetMask != 0)))
        {
            result = esp_netif_dhcpc_stop(_stationNetIfHandle);
            if (result == ESP_OK)
            {
                char ip[INET_ADDRSTRLEN], subnet[INET_ADDRSTRLEN], gateway[INET_ADDRSTRLEN];
                strcpy(ip, inet_ntoa(accessPointInformation->IpAddress));
                strcpy(subnet, inet_ntoa(accessPointInformation->SubnetMask));
                strcpy(gateway, inet_ntoa(accessPointInformation->Gateway));
                TRACE_MESSAGE("Configuring static IP address, IP: %s, subnet mask %s, default gateway %s", ip, subnet, gateway);

                esp_netif_ip_info_t ip_info;
                ip_info.ip.addr = accessPointInformation->IpAddress;
                ip_info.gw.addr = accessPointInformation->Gateway;
                ip_info.netmask.addr = accessPointInformation->SubnetMask;
                result = esp_netif_set_ip_info(_stationNetIfHandle, &ip_info);
            }
        }

        if (result == ESP_OK)
        {
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            result = esp_wifi_init(&cfg);
        }

        if (result == ESP_OK)
        {
            result = esp_wifi_restore();
        }

        if (result == ESP_OK)
        {
            //
            //  esp_wifi_set_country_code() derives the channel range (schan/nchan) for the
            //  given country from ESP-IDF's own table.
            //
            //  _countryCode is validated on both read (constructor) and write
            //  (SetCountryCode), so it is always one of _validCountryCodes here.
            //
            result = esp_wifi_set_country_code(_countryCode.load(), false);
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
            result = esp_wifi_set_mode(mode);
        }

        if ((result == ESP_OK) && ((mode == WIFI_MODE_STA) || (mode == WIFI_MODE_APSTA)))
        {
            //
            //  Copied under the lock, so a SetMacAddress on the System task cannot leave half
            //  an old address and half a new one to apply.
            //
            uint8_t stationMacAddress[sizeof(_stationMacAddressOverride)];
            bool stationMacAddressIsSet = false;
            {
                MutexLock lock(StationMacAddressOverrideMutex());
                stationMacAddressIsSet = _stationMacAddressOverrideIsSet;
                memcpy(stationMacAddress, _stationMacAddressOverride, sizeof(stationMacAddress));
            }
            if (stationMacAddressIsSet)
            {
                result = esp_wifi_set_mac(WIFI_IF_STA, stationMacAddress);
            }
        }

        if ((result == ESP_OK) && _softApMacAddressOverrideIsSet && ((mode == WIFI_MODE_AP) || (mode == WIFI_MODE_APSTA)))
        {
            result = esp_wifi_set_mac(WIFI_IF_AP, _softApMacAddressOverride);
        }

        if (result == ESP_OK)
        {
            result = esp_wifi_start();
        }

        if (result != ESP_OK)
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT);
        }
    }
    ESP_ERROR_CHECK(result);

    TRACE_MESSAGE("StartWiFiInterface: Exit");
    return (result);
}

/**
 *  @brief Get the full list of available networks.
 *
 *  @pre
 *      The network interface must be started before this method is called.
 *      It is not necessary to connected to an access pint but the interface
 *      must be started.
 *
 *  @param message
 *      Request sent from the STM32.
 */
void WiFiRequestHandler::ScanForAccessPoints(Message *message)
{
    TRACE_MESSAGE("ScanForAccessPoints: Enter");
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    _currentRetryCount = 0;
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_CONNECTED_BIT))
    {
        TRACE_MESSAGE("Starting network");
        if (StartWiFiInterface(WIFI_MODE_STA, nullptr) != ESP_OK)
        {
            result = StatusCodes::CannotStartNetworkInterface;
        }
    }

    if (result == StatusCodes::CompletedOk)
    {
        bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
        if (bits & WIFI_INTERFACE_STARTED_BIT)
        {
            //
            //  WIFI_SCAN_COMPLETE_BIT is waited on with xClearOnExit = pdFALSE, so a bit left
            //  set by an earlier scan would make the wait below return immediately against an
            //  incomplete scan.
            //
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT);
            ESP_ERROR_CHECK(esp_wifi_scan_start(nullptr, true));

            bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
            if (bits & WIFI_SCAN_COMPLETE_BIT)
            {
                uint16_t apCount = 0;
                esp_wifi_scan_get_ap_num(&apCount);
                TRACE_MESSAGE("ScanForAccessPoints: Number of access points found: %" PRIu16, apCount);
                result = StatusCodes::CompletedOk;
                if (apCount != 0)
                {
                    //
                    //  Every block is checked before it is used, and all of them are released on
                    //  every path out.  If the records cannot be copied out, the driver's own copy
                    //  of the scan results is released instead, as esp_wifi_scan_get_ap_records
                    //  would otherwise have done.
                    //
                    Esp32Messaging::AccessPointList *accessPointList = static_cast<Esp32Messaging::AccessPointList *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPointList)));
                    wifi_ap_record_t *accessPoints = static_cast<wifi_ap_record_t *>(pvPortMalloc(apCount * sizeof(wifi_ap_record_t)));
                    Esp32Messaging::AccessPoint *accessPoint = static_cast<Esp32Messaging::AccessPoint *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPoint)));
                    if ((accessPointList == nullptr) || (accessPoints == nullptr) || (accessPoint == nullptr))
                    {
                        esp_wifi_clear_ap_list();
                        result = StatusCodes::EspOutOfMemory;
                    }
                    else
                    {
                        memset(accessPointList, 0, sizeof(Esp32Messaging::AccessPointList));
                        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, accessPoints));
                        accessPointList->NumberOfAccessPoints = apCount;

                        int singleAccessPointSize = Encoders::EncodedAccessPointBufferSize(accessPoint);
                        accessPointList->AccessPointsLength = singleAccessPointSize * apCount;
                        accessPointList->AccessPoints = static_cast<uint8_t *>(pvPortMalloc(accessPointList->AccessPointsLength));
                        if (accessPointList->AccessPoints == nullptr)
                        {
                            result = StatusCodes::EspOutOfMemory;
                        }
                        else
                        {
                            uint8_t *currentLocation = accessPointList->AccessPoints;
                            for (int index = 0; index < apCount; index++)
                            {
                                memset(accessPoint, 0, sizeof(Esp32Messaging::AccessPoint));
                                memcpy(accessPoint->Ssid, &accessPoints[index].ssid, sizeof(accessPoint->Ssid));
                                memcpy(accessPoint->Bssid, &accessPoints[index].bssid, 6);
                                accessPoint->PrimaryChannel = accessPoints[index].primary;
                                accessPoint->SecondaryChannel = accessPoints[index].second;
                                accessPoint->Rssi = accessPoints[index].rssi;
                                accessPoint->AuthenticationMode = accessPoints[index].authmode;
                                accessPoint->Protocols = accessPoints[index].phy_11b | accessPoints[index].phy_11g | accessPoints[index].phy_11n | accessPoints[index].phy_lr | accessPoints[index].wps;
                                Encoders::EncodeAccessPoint(accessPoint, currentLocation);
                                currentLocation += singleAccessPointSize;
                            }

                            message->PayloadLength = Encoders::EncodedAccessPointListBufferSize(accessPointList);
                            message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
                            if (message->Payload == nullptr)
                            {
                                message->PayloadLength = 0;
                                result = StatusCodes::EspOutOfMemory;
                            }
                            else
                            {
                                Encoders::EncodeAccessPointList(accessPointList, message->Payload);
                            }
                        }
                        Utils::DeleteAndNullifyPointer(accessPointList->AccessPoints);
                    }
                    Utils::DeleteAndNullifyPointer(accessPoints);
                    Utils::DeleteAndNullifyPointer(accessPoint);
                    Utils::DeleteAndNullifyPointer(accessPointList);
                }
            }
            else
            {
                TRACE_MESSAGE("ScanForAccessPoints: Scan timeout.");
                result = StatusCodes::Timeout;
            }
        }
        else
        {
            TRACE_MESSAGE("ScanForAccessPoints: Scan timeout.");
            result = StatusCodes::Timeout;
        }
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
    TRACE_MESSAGE("ScanForAccessPoints: Exit");
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
    {
        MutexLock lock(EventStateMutex());
        memcpy(&_ipInformation, &eventData->ip_info, sizeof(esp_netif_ip_info_t));
    }

    if (_useDhcp)
    {
        RaiseNetworkGotIpEvent();
    }

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT | WIFI_INTERFACE_STARTING_BIT);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_HAVE_IP_BIT);
    TRACE_MESSAGE("StationGotIpEvent: Exit");
}

/**
 *  @brief Process the station started event.
 */
void WiFiRequestHandler::StationStartedEvent()
{
    TRACE_MESSAGE("StationStartedEvent: Enter");
    //
    //  A copy, as the System task can replace the stored name at any time (issue #168).
    //
    const std::string deviceName = SystemRequestHandler::GetDeviceName();
    const esp_err_t hostnameResult = esp_netif_set_hostname(_stationNetIfHandle, deviceName.empty() ? "Meadow" : deviceName.c_str());
    if (hostnameResult != ESP_OK)
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_INTERFACE_STARTING_BIT | WIFI_CONNECTING_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
    }
    else
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT | WIFI_ERROR_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT);
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
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_INTERFACE_STARTING_BIT);

    TRACE_MESSAGE("StationStoppedEvent: Exit");
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
    {
        MutexLock lock(EventStateMutex());
        memcpy(&_apInformation, eventData, sizeof(wifi_event_sta_connected_t));
    }

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);

    RaiseConnectToAccessPointEvent(StatusCodes::CompletedOk);

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

    {
        MutexLock lock(EventStateMutex());
        memcpy(&_disconnectData, eventData, sizeof(wifi_event_sta_disconnected_t));
    }

    bool raiseDisconnectedEvent = true;
    EventBits_t bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_DISCONNECT_REQUESTED_BIT, pdFALSE, pdFALSE, 0);
    if (bits & WIFI_DISCONNECT_REQUESTED_BIT)
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_DISCONNECT_REQUESTED_BIT);
    }
    else
    {
        //
        //  Deal with working out if we should start the reconnection process as a forced disconnect has
        //  not been requested.
        //
        uint32_t maximumRetryCount = GetMaximumRetryCount();
        if (bits & WIFI_CONNECTED_BIT)
        {
            //
            //  So here we were connected and now we are not so clear the connection flags.
            //
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);
            _currentRetryCount = 0;
            RaiseDisconnectEvent();
        }
        //
        //  Read once, so the check, the first-attempt test and the trace all see the same
        //  count even if the WiFi task resets it meanwhile.
        //
        const uint32_t retryCount = _currentRetryCount.load();
        if (GetAutomaticReconnect() && (retryCount < maximumRetryCount))
        {
            TRACE_MESSAGE("StationDisconnectedEvent: Reconnecting, retry count %" PRIu32 ", maximum %" PRIu32, retryCount, maximumRetryCount);
            //
            //  the system is configured to automatically reconnect and we have not exceeded the retry count.
            //
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
            if (retryCount == 0)
            {
                RaiseNetworkConnectingEvent();
                _progressiveDelay->Restart();
            }
            esp_wifi_connect();
            _currentRetryCount++;
            raiseDisconnectedEvent = false;
            TRACE_MESSAGE("StationDisconnectedEvent: Delaying for 500ms");
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        else
        {
            TRACE_MESSAGE("StationDisconnectedEvent: Not reconnecting, retry count %" PRIu32 ", maximum %" PRIu32, retryCount, maximumRetryCount);
            //
            //  Here we were connected or we have been trying to connect and there two possibilities:
            //      1.  We are not configured to automatically reconnect.
            //      2.  We are configured to automatically reconnect but we have exceeded the retry count.
            //
            //  In the second case we need to raise the event to the application to let it know that we have
            //  exceeded the retry count.
            //
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
            if (GetAutomaticReconnect())
            {
                TRACE_MESSAGE("StationDisconnectedEvent: Retry count exceeded, raising event");
                RaiseNetworkRetryCountExceededEvent();
            }
        }
    }

    if (raiseDisconnectedEvent)
    {
        RaiseDisconnectEvent();
        {
            MutexLock lock(EventStateMutex());
            memset(&_apInformation, 0, sizeof(wifi_event_sta_connected_t));
            memset(&_ipInformation, 0, sizeof(_ipInformation));
        }
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_HAVE_IP_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
    }

    TRACE_MESSAGE("StationDisconnectedEvent: Exit");
}