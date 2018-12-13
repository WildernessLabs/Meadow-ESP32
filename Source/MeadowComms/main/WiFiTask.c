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

//
//  Handle for the WiFi messages to be processed by the WIFi task.
//
QueueHandle_t xWiFiQueueHandle = NULL;

//
//  Handle for the WiFi task.
//
TaskHandle_t xWiFiTaskHandle = NULL;

//
//  Event group containing 24-bits used to indicate the state
//  of the WiFi connection.
//
static EventGroupHandle_t xWiFiEventGroup;

const char *WIFI_TASK_NAME = "WiFiTask";

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
            xEventGroupSetBits(xWiFiEventGroup, WIFI_CONNECTED_BIT);
            xEventGroupClearBits(xWiFiEventGroup, WIFI_BUSY_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            if (_automaticReconnect)
            {
                xEventGroupClearBits(xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
                if (_currentRetryCount < _maximumRetryCount)
                {
                    xEventGroupSetBits(xWiFiEventGroup, WIFI_BUSY_BIT | WIFI_RECONNECTING_BIT);
                    esp_wifi_connect();
                    _currentRetryCount++;
                }
            }
            else
            {
                //
                //  TODO: Send disconnect message ?
                //
                xEventGroupClearBits(xWiFiEventGroup, WIFI_BUSY_BIT | WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
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

/// <summary>
///     Start the wireless hardware.
/// </summary>
/// <param name="message">Request sent from the STM32.</param>
void StartNetwork(struct ApplicationRequest *message)
{
    if (_wifiResponse != NULL)
    {
        // TODO: Deal with this error.
        vPortFree(_wifiResponse);
    }
    _wifiResponse = pvPortMalloc(sizeof(struct ApplicationResponse));
    _wifiResponse->interface = message->interface;
    _wifiResponse->messageID = message->messageID;
    _wifiResponse->code = 0;

    EventBits_t bits = xEventGroupGetBits(xWiFiEventGroup);
    if (!(bits & WIFI_INITIALISED_BIT))
    {
        char *ssid = (char *) message->payload;
        if ((ssid == NULL) || (strlen(ssid) > MAXIMUM_SSID_NAME_LENGTH))
        {
            SendResponse(ERR_INVALID_SSID);
            return;
        }

        char *password = (char *) (message->payload + strlen(ssid) + 1);
        if (strlen(password) > MAXIMUM_PASSWORD_LENGTH)
        {
            SendResponse(ERR_INVALID_PASSWORD);
            return;
        }

        tcpip_adapter_init();
        ESP_ERROR_CHECK(esp_event_loop_init(WiFiEventHandler, NULL));

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config = 
        {
            .sta =
            {
                .scan_method = DEFAULT_SCAN_METHOD,
                .sort_method = DEFAULT_SORT_METHOD,
                .threshold.rssi = DEFAULT_RSSI,
                .threshold.authmode = DEFAULT_AUTHMODE,
            },
        };
        memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
        memcpy(wifi_config.sta.password, password, strlen(password));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        xEventGroupSetBits(xWiFiEventGroup, WIFI_BUSY_BIT | WIFI_INITIALISED_BIT | WIFI_CONNECTING_BIT);
    }
    else
    {
        SendResponse(ERR_WIFI_ALREADY_STARTED);
    }
}

/// <summary>
///     Get the full list of available networks.
/// </summary>
/// <remarks>
///     The network must be started before this method is called.
/// </remarks>
/// <param name="message">Request sent from the STM32.</param>
void GetAvailableNetworks(struct ApplicationRequest *message)
{
    if (_wifiResponse != NULL)
    {
        // TODO: Deal with this error.
        vPortFree(_wifiResponse);
    }
    _wifiResponse = pvPortMalloc(sizeof(struct ApplicationResponse));
    _wifiResponse->interface = message->interface;
    _wifiResponse->messageID = message->messageID;
    _wifiResponse->code = 0;

    wifi_scan_config_t scanConf =
    {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));
}


/// <summary>
///     Perform a HTTP GET request.
/// </summary>
/// <remarks>
///     The network must be started before this method is called.
/// </remarks>
/// <param name="message">Request sent from the STM32.</param>
void HttpGet(struct ApplicationRequest *message)
{
    const struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM, };
    struct addrinfo *addressInformation;
    struct in_addr *address;
    int socketHandle, r;
    char receiveBuffer[64];

    //
    //  TODO: Perform some error checking here.
    //

    char portNumber[6];
    utoa(ExtractUInt16(message->payload), portNumber, 10);
    //
    //  The server name (e.g. google.com) follows the 16-bit port number in the payload.
    //
    char *serverName = (char *) message->payload + 2;
    //
    //  The full URI being requested follows the server name (remember there server name
    //  is terminated by a 0 byte.
    //
    char *uri = serverName + strlen(serverName) + 1;
    char *request = pvPortMalloc(strlen(serverName) + strlen(uri) + 60);
    sprintf(request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\n\r\n", serverName, uri);
    ESP_LOGI(__func__, "HTTP Get Request");
    ESP_LOGI(__func__, "    Host: %s", serverName);
    ESP_LOGI(__func__, "    URI: %s", uri);
   
    xEventGroupWaitBits(xWiFiEventGroup, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    int errorCode = getaddrinfo(serverName, "80", &hints, &addressInformation);

    if ((errorCode != 0) || (addressInformation == NULL))
    {
        ESP_LOGE(__func__, "    DNS lookup failed err: %d addressInformation: %p", errorCode, addressInformation);
        return;
    }
    //
    //  Perform DNS lookup.
    //
    address = &((struct sockaddr_in *) addressInformation->ai_addr)->sin_addr;
    socketHandle = socket(addressInformation->ai_family, addressInformation->ai_socktype, 0);
    if (socketHandle < 0)
    {
        ESP_LOGE(__func__, "    Failed to allocate socket.");
        freeaddrinfo(addressInformation);
        return;
    }
    if (connect(socketHandle, addressInformation->ai_addr, addressInformation->ai_addrlen) != 0)
    {
        ESP_LOGE(__func__, "    Socket connect failed errno: %d", errno);
        close(socketHandle);
        freeaddrinfo(addressInformation);
        return;
    }

    freeaddrinfo(addressInformation);

    if (write(socketHandle, request, strlen(request)) < 0)
    {
        ESP_LOGE(__func__, "    Socket send failed");
        close(socketHandle);
        return;
    }

    struct timeval timeOut;
    timeOut.tv_sec = 5;
    timeOut.tv_usec = 0;
    if (setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &timeOut, sizeof(timeOut)) < 0)
    {
        ESP_LOGE(__func__, "    Failed to set socket receiving timeout");
        close(socketHandle);
        return;
    }

    do
    {
        bzero(receiveBuffer, sizeof(receiveBuffer));
        r = read(socketHandle, receiveBuffer, sizeof(receiveBuffer) - 1);
        for (int i = 0; i < r; i++)
        {
            putchar(receiveBuffer[i]);
        }
    }
    while (r > 0);
    printf("\n");

    ESP_LOGI(__func__, "    Finished reading from socket. Last read return: %d errno: %d\r\n", r, errno);
    close(socketHandle);

    vPortFree(request);
    _wifiResponse = pvPortMalloc(sizeof(struct ApplicationResponse));
    _wifiResponse->interface = message->interface;
    _wifiResponse->messageID = message->messageID;
    _wifiResponse->payload = NULL;
    _wifiResponse->payloadLength = 0;
    SendResponse(ERR_COMPLETED_OK);
}

/// <summary>
///     Task to process the WiFi messages.
/// <summary>
void WiFiTask(void *pvParameters)
{
    while (true)
    {
        struct ApplicationRequest message;
        BaseType_t result = xQueueReceive(xWiFiQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            switch (message.function)
            {
                case WIFI_FUNC_START:
                    StartNetwork(&message);
                    break;
                case WIFI_FUNC_GET_ACCESS_POINTS:
                    GetAvailableNetworks(&message);
                    break;
                case WIFI_FUNC_HTTP_GET:
                    HttpGet(&message);
                    break;
                default:
                    ESP_LOGI(__func__, "WiFi message received, function %d", message.function);
                    //
                    //  Memory for the payload is allocated on the heap and it needs to be released.
                    //
                    if (message.payload != NULL)
                    {
                        ESP_LOG_BUFFER_HEXDUMP(__func__, message.payload, message.payloadLength, ESP_LOG_INFO);
                    }
                    break;
            }
            if (message.payload != NULL)
            {
                vPortFree(message.payload);
            }
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xWiFiTaskHandle);
}
