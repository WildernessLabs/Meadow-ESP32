#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include <string.h>

#include "WiFiTask.h"
#include "Messaging.h"
#include "UARTTask.h"
#include "Encoders.h"
#include "ErrorCodes.h"
#include "Tasks.hpp"

//
//  Handle for the WiFi messages to be processed by the WIFi task.
//
QueueHandle_t xWiFiQueueHandle = NULL;

//
//  Event group containing 24-bits used to indicate the state
//  of the WiFi connection.
//
static EventGroupHandle_t xWiFiEventGroup;

//
//  Current retry count.
//
static int _currentRetryCount = 0;

//
//  Maximum number of retries for a failed operation.
//
static int _maximumRetryCount = DEFAULT_MAXIMUM_RETRY_COUNT;

//
//  Indicate if the system should automatically reconnect to the WiFi
//  if we disconnect for any reason.
//
static bool _automaticReconnect = true;

//
//  Response message for the current request.
//
static struct ApplicationResponse *_wifiResponse = NULL;

/// <summary>
///     Setup the WiFi system and task.
/// </summary>
void SetupWiFi()
{
    xWiFiEventGroup = xEventGroupCreate();
    xTaskCreate(WiFiTask, WIFI_TASK_NAME, 2048 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
}

/// <summary>
///     Send the response and clean up the WiFi response object.
/// </summary>
/// <param name="statusCode">Response code to be sent back to the STM32</param>
void SendResponse(uint32_t statusCode)
{
    _wifiResponse->code = statusCode;
    QueueUARTResponse(_wifiResponse);
    vPortFree(_wifiResponse);
    _wifiResponse = NULL;
}

/// <summary>
///     Process the scan results.
/// </summary>
/// <param name="event">Event information passed to the event loop.</param>
void ProcessAccessPointScanCompletion(system_event_t *event)
{
    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);
    ESP_LOGI(__func__, "Number of access points found: %d\n", apCount);
    if (apCount == 0)
    {
        _wifiResponse->payload = NULL;
        _wifiResponse->payloadLength = 0;
    }
    else
    {
        //
        //  First two bytes in this payload are the number of access points.
        //
        _wifiResponse->payloadLength = (ACCESS_POINT_INFORMATION_LENGTH * apCount) + 2;
        _wifiResponse->payload = pvPortMalloc(_wifiResponse->payloadLength);
        EncodeUInt16(apCount, _wifiResponse->payload);
        //
        //  Now we get the access point information.
        //
        wifi_ap_record_t *accessPoints = pvPortMalloc(apCount * sizeof(wifi_ap_record_t));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, accessPoints));
        uint8_t *destination = _wifiResponse->payload + 2;
        for (int index = 0; index < apCount; index++)
        {
            memcpy(destination + BSSID_OFFSET, &accessPoints[index].bssid, 6);
            memcpy(destination + SSID_OFFSET, &accessPoints[index].ssid, 33);
            *(destination + PRIMARY_CHANNEL_OFFSET) = accessPoints[index].primary;
            *(destination + SECONDARY_CHANNEL_OFFSET) = accessPoints[index].second ;
            *(destination + RSSI_OFFSET) = accessPoints[index].rssi;
            *(destination + AUTHENTICATION_MODE_OFFSET) = accessPoints[index].authmode;
            *(destination + PAIRWISE_CIPHER_OFFSET) = accessPoints[index].pairwise_cipher;
            *(destination + GROUP_CIPHER_OFFSET) = accessPoints[index].group_cipher;
            *(destination + ANTENNA_OFFSET) = accessPoints[index].ant;
            *(destination + PROTOCOLS_OFFSET) = accessPoints[index].phy_11b | accessPoints[index].phy_11g | accessPoints[index].phy_11n | accessPoints[index].phy_lr | accessPoints[index].wps;
            memcpy(destination + COUNTRY_CODE_OFFSET, accessPoints[index].country.cc, 3);
            *(destination + START_CHANNEL_OFFSET) = accessPoints[index].country.schan;
            *(destination + NUMBER_OF_CHANNELS_OFFSET) = accessPoints[index].country.nchan;
            *(destination + TX_POWER_MAXIMUM_OFFSET) = accessPoints[index].country.max_tx_power ;
            *(destination + POLICY_OFFSET) = accessPoints[index].country.policy;
            destination += ACCESS_POINT_INFORMATION_LENGTH;
        }
        vPortFree(accessPoints);
    }
    SendResponse(ERR_COMPLETED_OK);
}

/// <summary>
///     Event handler for the WiFi system.
/// </summary>
/// <param name="ctx"></param>
/// <param name="event">Information relevant to the event being raised.</param>
static esp_err_t WiFiEventHandler(void *ctx, system_event_t *event)
{
    EventBits_t bits;

    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            _currentRetryCount = 0;
            bits = xEventGroupGetBits(xWiFiEventGroup);
            //
            //  TODO: We can sometimes get here when _wifiResponse is NULL, need to review this.
            //
            if ((_wifiResponse != NULL) && (bits & WIFI_CONNECTING_BIT))
            {
                _wifiResponse->payloadLength = sizeof(ip4_addr_t);
                _wifiResponse->payload = pvPortMalloc(_wifiResponse->payloadLength);
                memcpy(_wifiResponse->payload, &event->event_info.got_ip.ip_info.ip, _wifiResponse->payloadLength);
                SendResponse(ERR_COMPLETED_OK);
            }
            xEventGroupSetBits(xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_READY_BIT);
            xEventGroupClearBits(xWiFiEventGroup, WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            if (_automaticReconnect)
            {
                xEventGroupClearBits(xWiFiEventGroup, WIFI_READY_BIT | WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
                if (_currentRetryCount < _maximumRetryCount)
                {
                    xEventGroupSetBits(xWiFiEventGroup, WIFI_RECONNECTING_BIT);
                    esp_wifi_connect();
                    _currentRetryCount++;
                }
            }
            else
            {
                //
                //  TODO: Send disconnect message ?
                //
                xEventGroupClearBits(xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
                xEventGroupSetBits(xWiFiEventGroup, WIFI_READY_BIT);
            }
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            ProcessAccessPointScanCompletion(event);
            break;
        default:
            break;
    }
    return ESP_OK;
}
