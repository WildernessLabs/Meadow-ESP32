/*
 *  AccessPoint.cpp
 *
 *  Implement the access point functionality (the board acting as an access point).
 */
#include "../../build/config/sdkconfig.h"

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

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Extract and validate the access point information.
 * 
 * @param data
 *      Pointer to a block of memory holding the encoded access point information. 
 * 
 * @return Esp32Messaging::AccessPointInformation* 
 *      Pointer to validated access point information, nullptr if there is a problem.
 */
Esp32Messaging::AccessPointInformation *WiFiRequestHandler::ExtractAndValidateAccessPointInformation(uint8_t *data)
{
    Esp32Messaging::AccessPointInformation *result = nullptr;

    if (data)
    {
        result = Encoders::ExtractAccessPointInformation(data);
        if (result)
        {
            bool error = (!result->NetworkName) || (strlen(result->NetworkName) == 0) || (strlen(result->NetworkName) > MAXIMUM_SSID_LENGTH);
            error |= (!result->Password) || (strlen(result->Password) > MAXIMUM_PASSWORD_LENGTH);
            error |= (result->IpAddress == 0) || (result->SubnetMask == 0) || (result->Gateway == 0);
            if (error)
            {
                vPortFree(result);
                result = nullptr;
            }
        }
    }
    
    return(result);
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

    Esp32Messaging::AccessPointInformation *accessPointInformation = ExtractAndValidateAccessPointInformation(message->Payload);
    message->DeletePayload();

    if (accessPointInformation)
    {
        EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
        uint32_t ipAddress = accessPointInformation->IpAddress;
        uint32_t subnetMask = accessPointInformation->SubnetMask;
        uint32_t gateway = accessPointInformation->Gateway;
        if (!(bits & WIFI_INTERFACE_STARTED_BIT))
        {
            accessPointInformation->IpAddress = 0;
            accessPointInformation->SubnetMask = 0;
            accessPointInformation->Gateway = 0;
            if (StartWiFiInterface(WIFI_MODE_APSTA, accessPointInformation) != ESP_OK)
            {
                result = StatusCodes::CannotStartNetworkInterface;
            }
            else
            {
                TRACE_MESSAGE("WiFi interface started OK.");
            }
            bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
            if ((bits & WIFI_INTERFACE_STARTED_BIT) == 0)
            {
                result = StatusCodes::CannotStartNetworkInterface;
            }
        }
        else
        {
            if (bits & WIFI_ACCESS_POINT_STARTED_BIT)
            {
                result = StatusCodes::AccessPointAlreadyStarted;
            }
        }
        if (result == StatusCodes::CompletedOk)
        {
            wifi_config_t config = { };
            config.ap.ssid_len = strlen(accessPointInformation->NetworkName);
            memcpy(config.ap.ssid, accessPointInformation->NetworkName, strlen(accessPointInformation->NetworkName));
            memcpy(config.ap.password, accessPointInformation->Password, strlen(accessPointInformation->Password));
            config.ap.max_connection = 10;
            config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            esp_err_t error = esp_wifi_set_config(ESP_IF_WIFI_AP, &config);
            if (error != ESP_OK)
            {
                TRACE_MESSAGE("Setting access point configuration returned 0x%x (%s)", error, esp_err_to_name(error));
                result = StatusCodes::CannotStartAccessPoint;
            }
            else
            {
                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
                if ((bits & WIFI_ACCESS_POINT_STARTED_BIT) == 0)
                {
                    result = StatusCodes::Timeout;
                    xEventGroupClearBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
                }
            }
        }
        vPortFree(accessPointInformation);
    }
    else
    {
        result = StatusCodes::CannotStartAccessPoint;
    }

    message->PayloadLength = 0;
    message->Payload = nullptr;
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

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT);

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
 * @brief Process the node connecting to access point event.
 */
void WiFiRequestHandler::NodeConnectedEvent()
{
    TRACE_MESSAGE("NodeConnectedEvent: Enter");

    wifi_sta_list_t wifi_sta_list;
    tcpip_adapter_sta_list_t adapter_sta_list;
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);
    if (adapter_sta_list.num > 0)
    {
        TRACE_MESSAGE("%d stations connected", adapter_sta_list.num );
    }
    for (uint8_t i = 0; i < adapter_sta_list.num; i++)
    {
        tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
        TRACE_MESSAGE("Device %d MAC: " MACSTR " IP: 0x%08x, " IPSTR, i, MAC2STR(station.mac), (uint32_t) station.ip.addr, IP2STR(&station.ip));
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

    TRACE_MESSAGE("station " MACSTR " leave, AID=%d", MAC2STR(eventData->mac), eventData->aid);

    TRACE_MESSAGE("NodeDisconnectedEvent: Exit");
}
