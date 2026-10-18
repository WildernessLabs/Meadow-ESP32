/**
 * @file AccessPoint.cpp
 * @author Mark Stevens
 * @brief Implement the access point functionality (the board acting as an access point).
 * @date 2022-12-27
 *
 * @copyright Copyright (c) 2022 Wilderness Labs
 */

#include "sdkconfig.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include <string.h>

#include "WiFiRequestHandler.hpp"
#include "Logging.hpp"
#include "Utils.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Lock the _connectedNodes object.
 */
void WiFiRequestHandler::LockConnectedNodesMap()
{
    xSemaphoreTake(_connectedNodesListMutex, portMAX_DELAY);
}

/**
 *  @brief Unlock the _connectedNodes object.
 */
void WiFiRequestHandler::UnlockConnectedNodesMap()
{
    xSemaphoreGive(_connectedNodesListMutex);
}

/**
 * @brief Extract and validate the access point information.
 *
 * @param data
 *      Pointer to a block of memory holding the encoded access point information.
 *
 * @param dataLength
 *      Number of bytes in that block, the message's PayloadLength.
 *
 * @param status
 *      Why nullptr was returned: InvalidWiFiCredentials for a password the soft-AP cannot
 *      be started with, CannotStartAccessPoint for anything else wrong with the request.
 *
 * @return Esp32Messaging::AccessPointInformation*
 *      Pointer to validated access point information, nullptr if there is a problem.
 */
Esp32Messaging::AccessPointInformation *WiFiRequestHandler::ExtractAndValidateAccessPointInformation(uint8_t *data, const uint32_t dataLength, StatusCodes::StatusCodes &status)
{
    Esp32Messaging::AccessPointInformation *result = nullptr;
    status = StatusCodes::CannotStartAccessPoint;

    if (data)
    {
        result = Encoders::ExtractAccessPointInformation(data, dataLength);
        if (result)
        {
            const bool nameError = (!result->NetworkName) || (strlen(result->NetworkName) == 0) || (strlen(result->NetworkName) > MAXIMUM_SSID_LENGTH);
            // error |= (result->IpAddress == 0) || (result->SubnetMask == 0) || (result->Gateway == 0);
            //
            //  Refused before the interface is touched, so a request that cannot work is reported
            //  as the reason rather than as a soft-AP that would not start (issues #537 and #294).
            //
            StatusCodes::StatusCodes refusal = StatusCodes::CompletedOk;
            if (!nameError)
            {
                if (result->Channel > MAXIMUM_ACCESS_POINT_CHANNEL)
                {
                    refusal = StatusCodes::InvalidRequest;
                }
                else
                {
                    wifi_auth_mode_t authenticationMode = WIFI_AUTH_OPEN;
                    refusal = ResolveAccessPointAuthentication(result->Password, result->WiFiAuthenticationMode, authenticationMode);
                }
            }
            if (nameError || (refusal != StatusCodes::CompletedOk))
            {
                if (!nameError)
                {
                    status = refusal;
                }
                Encoders::FreeAccessPointInformation(&result);
            }
        }
    }

    return (result);
}

/**
 * @brief Is this a passphrase the soft-AP can be started with, for a mode that has one?
 *
 *  A passphrase is 8 to 63 characters.  A shorter one cannot start a working soft-AP.  64
 *  characters is refused until it is established on hardware that the soft-AP would take it as a
 *  hexadecimal key (issue #537).
 *
 * @param password
 *      Password to check, nullptr if there is none.
 *
 * @return true if the soft-AP can be started with the password.
 */
bool WiFiRequestHandler::IsValidAccessPointPassword(const char *password)
{
    bool result = false;

    if (password != nullptr)
    {
        const size_t length = strlen(password);
        result = (length >= MINIMUM_ACCESS_POINT_PASSPHRASE_LENGTH) && (length <= MAXIMUM_ACCESS_POINT_PASSPHRASE_LENGTH);
    }

    return (result);
}

/**
 * @brief Work out the authentication mode the soft-AP is started with (issue #294).
 *
 *  No password means an open soft-AP, whatever mode was asked for.  With a password the mode asked
 *  for is used.  Open with a password is refused rather than quietly leaving the soft-AP
 *  unprotected, and WEP and WPA2 Enterprise are not supported by a soft-AP.
 *
 * @param password
 *      Password asked for; nullptr, which is what an empty one is decoded as, and "" are no password.
 *
 * @param requestedMode
 *      WiFiAuthenticationMode asked for.
 *
 * @param authenticationMode
 *      Set to the mode the soft-AP is started with when the result is CompletedOk.
 *
 * @return StatusCodes::StatusCodes
 *      CompletedOk, InvalidWiFiCredentials for a password the mode cannot use, NotImplemented for a
 *      mode a soft-AP does not support, InvalidRequest for a mode that does not exist.
 */
StatusCodes::StatusCodes WiFiRequestHandler::ResolveAccessPointAuthentication(const char *password, const uint8_t requestedMode, wifi_auth_mode_t &authenticationMode)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    //
    //  An empty password is decoded as nullptr, so that is no password too.
    //
    if ((password == nullptr) || (strlen(password) == 0))
    {
        authenticationMode = WIFI_AUTH_OPEN;
    }
    else
    {
        switch (requestedMode)
        {
            case WiFiAuthenticationMode::Open:
                result = StatusCodes::InvalidWiFiCredentials;
                break;
            case WiFiAuthenticationMode::Wep:
            case WiFiAuthenticationMode::Wpa2Enterprise:
                result = StatusCodes::NotImplemented;
                break;
            case WiFiAuthenticationMode::WpaPsk:
                authenticationMode = WIFI_AUTH_WPA_PSK;
                break;
            case WiFiAuthenticationMode::Wpa2Psk:
                authenticationMode = WIFI_AUTH_WPA2_PSK;
                break;
            case WiFiAuthenticationMode::WpaWpa2Psk:
                authenticationMode = WIFI_AUTH_WPA_WPA2_PSK;
                break;
            case WiFiAuthenticationMode::Wpa3Psk:
                authenticationMode = WIFI_AUTH_WPA3_PSK;
                break;
            case WiFiAuthenticationMode::Wpa2Wpa3Psk:
                authenticationMode = WIFI_AUTH_WPA2_WPA3_PSK;
                break;
            default:
                result = StatusCodes::InvalidRequest;
                break;
        }
        if ((result == StatusCodes::CompletedOk) && !IsValidAccessPointPassword(password))
        {
            result = StatusCodes::InvalidWiFiCredentials;
        }
    }

    return (result);
}

/**
 * @brief Build the soft-AP configuration from the access point information asked for (issue #294).
 *
 *  The authentication mode, the channel and whether the name is hidden are the ones asked for.  A
 *  channel of 0 is left to the ESP32.
 *
 * @param information
 *      Access point information that has been through ExtractAndValidateAccessPointInformation.
 *
 * @param config
 *      Set to the soft-AP configuration.
 */
void WiFiRequestHandler::BuildAccessPointConfiguration(const Esp32Messaging::AccessPointInformation *information, wifi_config_t &config)
{
    config = {};

    const size_t networkNameLength = strlen(information->NetworkName);
    const size_t passwordLength = (information->Password != nullptr) ? strlen(information->Password) : 0;
    config.ap.ssid_len = static_cast<uint8_t>(networkNameLength);
    memcpy(config.ap.ssid, information->NetworkName, networkNameLength);
    if (passwordLength > 0)
    {
        memcpy(config.ap.password, information->Password, passwordLength);
    }
    config.ap.max_connection = ESP_WIFI_MAX_CONN_NUM;

    wifi_auth_mode_t authenticationMode = WIFI_AUTH_OPEN;
    ResolveAccessPointAuthentication(information->Password, information->WiFiAuthenticationMode, authenticationMode);
    config.ap.authmode = authenticationMode;
    config.ap.channel = information->Channel;
    config.ap.ssid_hidden = (information->Hidden != 0) ? 1 : 0;

    //
    //  WPA3 needs protected management frames, required for WPA3 alone and offered for WPA2/WPA3.
    //
    if ((authenticationMode == WIFI_AUTH_WPA3_PSK) || (authenticationMode == WIFI_AUTH_WPA2_WPA3_PSK))
    {
        config.ap.pmf_cfg.capable = true;
        config.ap.pmf_cfg.required = (authenticationMode == WIFI_AUTH_WPA3_PSK);
        config.ap.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    }
}

/**
 *  @brief Start the access point.
 *
 *  @param message
 *      Message from the C# application containing information about the access point configuration.
 */
void WiFiRequestHandler::StartAccessPoint(Message *message)
{
    TRACE_MESSAGE("StartAccessPoint: Enter");

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    StatusCodes::StatusCodes refusal = StatusCodes::CannotStartAccessPoint;
    Esp32Messaging::AccessPointInformation *accessPointInformation = ExtractAndValidateAccessPointInformation(message->Payload, message->PayloadLength, refusal);
    message->DeletePayload();

    if (accessPointInformation)
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT | WIFI_ERROR_BIT);
        EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
        uint32_t ipAddress = accessPointInformation->IpAddress;
        uint32_t subnetMask = accessPointInformation->SubnetMask;
        uint32_t gateway = accessPointInformation->Gateway;

        if (!(bits & WIFI_INTERFACE_STARTED_BIT))
        {
            //
            //  Clear the IP settings so the station interface uses DHCP; the AP's
            //  static-IP settings are applied separately below.
            //
            accessPointInformation->IpAddress = 0;
            accessPointInformation->SubnetMask = 0;
            accessPointInformation->Gateway = 0;
            if (StartWiFiInterface(WIFI_MODE_APSTA, accessPointInformation) != ESP_OK)
            {
                result = StatusCodes::CannotStartNetworkInterface;
            }
            else
            {
                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
                if ((bits & WIFI_INTERFACE_STARTED_BIT) == 0)
                {
                    result = StatusCodes::CannotStartNetworkInterface;
                }
                else
                {
                    TRACE_MESSAGE("StartAccessPoint: WiFi interface started OK.");
                }
            }
        }
        else
        {
            //
            //  Interface already started in station mode — upgrade to APSTA so
            //  station mode remains available alongside the soft-AP.
            //
            if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK)
            {
                result = StatusCodes::CannotStartNetworkInterface;
            }
            else
            {
                if (_softApNetIfHandle == nullptr)
                {
                    _softApNetIfHandle = esp_netif_create_default_wifi_ap();
                    if (_softApNetIfHandle == nullptr)
                    {
                        result = StatusCodes::CannotStartNetworkInterface;
                    }
                }
            }
        }

        if (result == StatusCodes::CompletedOk)
        {
            //
            //  The password is not traced: a trace would put it in the log in clear.
            //
            TRACE_MESSAGE("StartAccessPoint: Creating access point: %s", accessPointInformation->NetworkName);
            wifi_config_t config;
            BuildAccessPointConfiguration(accessPointInformation, config);
            esp_err_t error = esp_wifi_set_config(WIFI_IF_AP, &config);
            if (error != ESP_OK)
            {
                TRACE_MESSAGE("StartAccessPoint: Setting access point configuration returned 0x%x (%s)", error, esp_err_to_name(error));
                result = StatusCodes::CannotStartAccessPoint;
            }
            else
            {
                if ((ipAddress != 0) && (gateway != 0) && (subnetMask != 0))
                {
                    if (esp_netif_dhcps_stop(_softApNetIfHandle) == ESP_OK)
                    {
                        char ipText[INET_ADDRSTRLEN], subnetMaskText[INET_ADDRSTRLEN], gatewayText[INET_ADDRSTRLEN];
                        inet_ntoa_r(ipAddress, ipText, sizeof(ipText));
                        inet_ntoa_r(subnetMask, subnetMaskText, sizeof(subnetMaskText));
                        inet_ntoa_r(gateway, gatewayText, sizeof(gatewayText));
                        TRACE_MESSAGE("StartAccessPoint: Configuring access point DHCP server, IP: %s, subnet mask %s, default gateway %s", ipText, subnetMaskText, gatewayText);

                        esp_netif_ip_info_t ipInformation = {};
                        ipInformation.ip.addr = ipAddress;
                        ipInformation.gw.addr = gateway;
                        ipInformation.netmask.addr = subnetMask;
                        if (esp_netif_set_ip_info(_softApNetIfHandle, &ipInformation) != ESP_OK)
                        {
                            result = StatusCodes::DhcpConfigurationError;
                        }
                        else
                        {
                            if (esp_netif_dhcps_start(_softApNetIfHandle) != ESP_OK)
                            {
                                result = StatusCodes::DhcpConfigurationError;
                            }
                        }
                    }
                    else
                    {
                        result = StatusCodes::DhcpConfigurationError;
                    }
                }

                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
                if ((bits & WIFI_ACCESS_POINT_STARTED_BIT) == 0)
                {
                    result = StatusCodes::Timeout;
                    xEventGroupClearBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
                }
            }
        }

        Encoders::FreeAccessPointInformation(&accessPointInformation);
    }
    else
    {
        result = refusal;
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    Logging::DumpMessage(COMPONENT_NAME, message);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("StartAccessPoint: Exit");
}

/**
 *  @brief Stop the access point if it is running.
 *
 *  @param message
 *      Message from the C# application requesting that the access point is stopped.
 */
void WiFiRequestHandler::StopAccessPoint(Message *message)
{
    TRACE_MESSAGE("StopAccessPoint: Enter");

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_ACCESS_POINT_STARTED_BIT))
    {
        result = StatusCodes::AccessPointNotStarted;
    }
    else
    {
        //
        //  Switch back to station-only mode, which stops the access point.  If the station
        //  connection is active the STA interface is preserved; otherwise the device returns
        //  to an idle station state.
        //
        //  The access point configuration is not cleared first.  Writing an empty one to a
        //  running access point restarts it, so an AccessPointStartedEvent followed the stop
        //  and the access point appeared to start again (issue #517).  StartAccessPoint sets
        //  the configuration every time, so nothing is gained by clearing it.
        //
        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        {
            result = StatusCodes::Failure;
        }

        //
        //  The access point network interface is kept, not destroyed and not set to nullptr
        //  (issue #292).  The access point's events still read it as it stops; destroying it
        //  here let one use freed memory.  Starting the access point again reuses the
        //  interface held, and StopWiFIAdapter() destroys it with the station one, after the
        //  event handlers are unregistered.
        //
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    Logging::DumpMessage(COMPONENT_NAME, message);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("StopAccessPoint: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Event Handlers
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Process the access point started event.
 */
void WiFiRequestHandler::AccessPointStartedEvent()
{
    TRACE_MESSAGE("AccessPointStartedEvent: Enter");

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    uint8_t *payload = nullptr;
    uint32_t payloadLength = 0;

    esp_netif_ip_info_t ipInformation = {};
    if (esp_netif_get_ip_info(_softApNetIfHandle, &ipInformation) == ESP_OK)
    {
        Esp32Messaging::AccessPointInformation accessPointInformation = {};
        accessPointInformation.IpAddress = ipInformation.ip.addr;
        accessPointInformation.SubnetMask = ipInformation.netmask.addr;
        accessPointInformation.Gateway = ipInformation.gw.addr;
        payloadLength = Encoders::EncodedAccessPointInformationBufferSize(&accessPointInformation);
        payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        if ((payload == nullptr) || !Encoders::EncodeAccessPointInformation(&accessPointInformation, payload, payloadLength))
        {
            Utils::DeleteAndNullifyPointer(payload);
            payloadLength = 0;
            result = StatusCodes::Failure;
        }
    }

    //
    //  RaiseEvent checks the message was made and releases the payload if it was not (issue #388).
    //
    RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::AccessPointStartedEvent, result, payload, payloadLength);

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT);

    TRACE_MESSAGE("AccessPointStartedEvent: Exit");
}

/**
 *  @brief Process the access point stopped event.
 */
void WiFiRequestHandler::AccessPointStoppedEvent()
{
    TRACE_MESSAGE("AccessPointStoppedEvent: Enter");

    LockConnectedNodesMap();
    _connectedNodes.clear();
    UnlockConnectedNodesMap();

    RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::AccessPointStoppedEvent, StatusCodes::CompletedOk);

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT);

    TRACE_MESSAGE("AccessPointStoppedEvent: Exit");
}

/**
 * @brief Raise an event for a node connecting or disconnecting.
 *
 * @param eventType
 *      Type of event to raise, NodeConnectedEvent or NodeDisconnectedEvent
 *
 * @param data
 *      Information about the node (MAC and possibly IP address).
 */
void WiFiRequestHandler::RaiseNodeConnectionChangeEvent(WiFiFunction::WiFiFunction eventType, Esp32Messaging::NodeConnectionChangeEventData *data)
{
    uint32_t payloadLength = Encoders::EncodedNodeConnectionChangeEventDataBufferSize(data);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    if (payload != nullptr)
    {
        Encoders::EncodeNodeConnectionChangeEventData(data, payload);
        RaiseEvent(Esp32Interfaces::WiFi, eventType, StatusCodes::CompletedOk, payload, payloadLength);
    }
    else
    {
        ERROR_MESSAGE("Cannot allocate the payload for event %s", Logging::FunctionName(Esp32Interfaces::WiFi, eventType));
    }
}

/**
 * @brief Process a node associating with the access point.
 *
 *        The node is added to the connected nodes with the IP address 0, so that it is in the
 *        list from the time it associates rather than only once it has a lease.  A node that
 *        never gets a lease is then found when it disconnects (issue #332).  Nothing is
 *        reported to the STM32 until the node is given an address.
 *
 * @param eventData
 *      Association event data, which carries the station MAC address.
 */
void WiFiRequestHandler::NodeAssociatedEvent(wifi_event_ap_staconnected_t *eventData)
{
    TRACE_MESSAGE("NodeAssociatedEvent: Enter");

    uint64_t key = 0;
    memcpy(&key, &eventData->mac, 6);

    LockConnectedNodesMap();

    // cppcheck-suppress unknownMacro
    TRACE_MESSAGE("NodeAssociatedEvent: MAC " MACSTR, MAC2STR(eventData->mac));

    //
    //  A node that already has an address keeps it.
    //
    if (_connectedNodes.find(key) == _connectedNodes.end())
    {
        _connectedNodes[key] = 0;
    }

    UnlockConnectedNodesMap();

    TRACE_MESSAGE("NodeAssociatedEvent: Exit");
}

/**
 * @brief Process the node connecting to access point event.
 *
 *        Called when the DHCP server has assigned an IP to a newly connected
 *        station.  The event data contains both the station MAC address and
 *        the assigned IP so no secondary STA-list query is required.  This
 *        completes the record NodeAssociatedEvent made, or makes it if the
 *        association was not seen.
 *
 * @param eventData
 *      DHCP-assignment event data — carries the station MAC and IP address.
 */
void WiFiRequestHandler::NodeConnectedEvent(ip_event_ap_staipassigned_t *eventData)
{
    TRACE_MESSAGE("NodeConnectedEvent: Enter");

    uint64_t key = 0;
    memcpy(&key, &eventData->mac, 6);

    // cppcheck-suppress unknownMacro
    TRACE_MESSAGE("NodeConnectedEvent: MAC " MACSTR " assigned IP " IPSTR, MAC2STR(eventData->mac), IP2STR(&eventData->ip));

    Esp32Messaging::NodeConnectionChangeEventData data = {};
    memcpy(&data.MacAddress, &eventData->mac, 6);
    memcpy(&data.IpAddress, &eventData->ip, sizeof(uint32_t));

    //
    //  The list is locked only while it is updated.  Queuing an event allocates, takes the
    //  outbound queue's mutex, writes to the UART and can wait for room in the queue, so it is
    //  done after the lock is released (issue #550).
    //
    LockConnectedNodesMap();
    _connectedNodes[key] = data.IpAddress;
    const bool maximumReached = (_connectedNodes.size() >= ESP_WIFI_MAX_CONN_NUM);
    UnlockConnectedNodesMap();

    RaiseNodeConnectionChangeEvent(WiFiFunction::NodeConnectedEvent, &data);

    if (maximumReached)
    {
        RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::MaximumConnectionsReachedEvent, StatusCodes::CompletedOk);
    }

    TRACE_MESSAGE("NodeConnectedEvent: Exit");
}

/**
 * @brief Processes the node disconnecting from the access point event.
 *
 * @param eventData
 *      Information about the node that has disconnected from the access point.
 */
void WiFiRequestHandler::NodeDisconnectedEvent(wifi_event_ap_stadisconnected_t *eventData)
{
    TRACE_MESSAGE("NodeDisconnectedEvent: Enter");

    uint64_t key = 0;
    memcpy(&key, &eventData->mac, 6);

    //
    //  The list is locked only while it is updated; the event is raised after it is released
    //  (issue #550).
    //
    LockConnectedNodesMap();
    std::map<uint64_t, uint32_t>::iterator it = _connectedNodes.find(key);
    const bool found = (it != _connectedNodes.end());
    //
    //  The STM32 was told of the node only if it was given an address, so only then is it
    //  told that the node has gone (issue #332).
    //
    const bool reported = found && (it->second != 0);
    if (found)
    {
        _connectedNodes.erase(it);
    }
    UnlockConnectedNodesMap();

    if (found)
    {
        TRACE_MESSAGE("NodeDisconnectedEvent: Found MAC address " MACSTR, MAC2STR(eventData->mac));
        if (reported)
        {
            Esp32Messaging::NodeConnectionChangeEventData data = {};
            memcpy(&data.MacAddress, &eventData->mac, 6);
            RaiseNodeConnectionChangeEvent(WiFiFunction::NodeDisconnectedEvent, &data);
        }
    }
    else
    {
        TRACE_MESSAGE("NodeDisconnectedEvent raised but the node could not be found in the connected nodes list.");
    }

    TRACE_MESSAGE("NodeDisconnectedEvent: Exit");
}
