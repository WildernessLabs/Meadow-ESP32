/*
 * WiFi.cpp
 *
 *  Created on: 16 Jan 2019
 *      Author: markstevens
 */
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

#include "Messaging.h"
#include "WiFiTask.h"
#include "WiFi.hpp"
#include "Encoders.h"
#include "ErrorCodes.h"

WiFi::WiFi()
{

}

WiFi::~WiFi()
{

}

void WiFi::DispatchRequest(struct ApplicationRequest *request)
{
    switch (request->function)
    {
        case WIFI_FUNC_START:
            StartNetwork(request);
            break;
        case WIFI_FUNC_GET_ACCESS_POINTS:
            GetAvailableNetworks(request);
            break;
        case WIFI_FUNC_HTTP_GET:
            HttpGet(request);
            break;
        default:
            ESP_LOGI(__func__, "Unknown WiFi message received, function %d", request->function);
            if (request->payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(__func__, request->payload, request->payloadLength, ESP_LOG_INFO);
            }
            break;
    }
    //
    //  Memory for the payload is allocated on the heap and it needs to be released.
    //
    if (request->payload != NULL)
    {
        vPortFree(request->payload);
    }

}

void WiFi::SetupWiFi()
{

}

/// <summary>
///     Start the wireless hardware.
/// </summary>
/// <param name="request">Request sent from the STM32.</param>
void WiFi::StartNetwork(struct ApplicationRequest *message)
{
//    if (_wifiResponse != NULL)
//    {
//        // TODO: Deal with this error.
//        vPortFree(_wifiResponse);
//    }
//    _wifiResponse = pvPortMalloc(sizeof(struct ApplicationResponse));
//    _wifiResponse->interface = message->interface;
//    _wifiResponse->messageID = message->messageID;
//    _wifiResponse->code = 0;
//
//    EventBits_t bits = xEventGroupGetBits(xWiFiEventGroup);
//    if (!(bits & WIFI_INITIALISED_BIT))
//    {
//        char *ssid = (char *) message->payload;
//        if ((ssid == NULL) || (strlen(ssid) > MAXIMUM_SSID_NAME_LENGTH))
//        {
//            SendResponse(ERR_INVALID_SSID);
//            return;
//        }
//
//        char *password = (char *) (message->payload + strlen(ssid) + 1);
//        if (strlen(password) > MAXIMUM_PASSWORD_LENGTH)
//        {
//            SendResponse(ERR_INVALID_PASSWORD);
//            return;
//        }
//
//        tcpip_adapter_init();
//        ESP_ERROR_CHECK(esp_event_loop_init(WiFiEventHandler, NULL));
//
//        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//
//        wifi_config_t wifi_config =
//        {
//            .sta =
//            {
//                .scan_method = DEFAULT_SCAN_METHOD,
//                .sort_method = DEFAULT_SORT_METHOD,
//                .threshold.rssi = DEFAULT_RSSI,
//                .threshold.authmode = DEFAULT_AUTHMODE,
//            },
//        };
//        memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
//        memcpy(wifi_config.sta.password, password, strlen(password));
//        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//        ESP_ERROR_CHECK(esp_wifi_start());
//        xEventGroupSetBits(xWiFiEventGroup, WIFI_INITIALISED_BIT | WIFI_CONNECTING_BIT);
//        xEventGroupClearBits(xWiFiEventGroup, WIFI_READY_BIT);
//    }
//    else
//    {
//        SendResponse(ERR_WIFI_ALREADY_STARTED);
//    }
}

/// <summary>
///     Get the full list of available networks.
/// </summary>
/// <remarks>
///     The network must be started before this method is called.
/// </remarks>
/// <param name="message">Request sent from the STM32.</param>
void WiFi::GetAvailableNetworks(struct ApplicationRequest *request)
{
    if (_wifiResponse != NULL)
    {
        // TODO: Deal with this error.
        vPortFree(_wifiResponse);
    }
    _wifiResponse = (struct ApplicationResponse *) pvPortMalloc(sizeof(struct ApplicationResponse));
    _wifiResponse->interface = request->interface;
    _wifiResponse->messageID = request->messageID;
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

void WiFi::HttpGet(struct ApplicationRequest *message)
{
//    const struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM, };
//    struct addrinfo *addressInformation;
//    struct in_addr *address;
//    int socketHandle, r;
//    char receiveBuffer[64];
//
//    //
//    //  TODO: Perform some error checking here.
//    //
//
//    char portNumber[6];
//    utoa(ExtractUInt16(message->payload), portNumber, 10);
//    //
//    //  The server name (e.g. google.com) follows the 16-bit port number in the payload.
//    //
//    char *serverName = (char *) message->payload + 2;
//    //
//    //  The full URI being requested follows the server name (remember there server name
//    //  is terminated by a 0 byte.
//    //
//    char *uri = serverName + strlen(serverName) + 1;
//    int maximumRequestLength = strlen(serverName) + strlen(uri) + 60;
//    char *request = pvPortMalloc(maximumRequestLength);
//    int actualRequestLength = snprintf(request, maximumRequestLength,  "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp32\r\n\r\n", uri, serverName);
//    if (actualRequestLength > (maximumRequestLength - 1))
//    {
//        ESP_LOGI(__func__, "Actual request length, %d,  would be too long for the allocated buffer %d.", actualRequestLength, actualRequestLength);
//    }
//    ESP_LOGI(__func__, "HTTP Get Request");
//    ESP_LOGI(__func__, "    Host: %s", serverName);
//    ESP_LOGI(__func__, "    URI: %s", uri);
//    ESP_LOGI(__func__, "    Request length: %d", strlen(request));
//    ESP_LOGI(__func__, "    Request: %s", request);
//
//    xEventGroupWaitBits(xWiFiEventGroup, WIFI_READY_BIT | WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
//
//    int errorCode = getaddrinfo(serverName, portNumber, &hints, &addressInformation);
//
//    if ((errorCode != 0) || (addressInformation == NULL))
//    {
//        ESP_LOGE(__func__, "    DNS lookup failed err: %d addressInformation: %p", errorCode, addressInformation);
//        return;
//    }
//    //
//    //  Perform DNS lookup.
//    //
//    address = &((struct sockaddr_in *) addressInformation->ai_addr)->sin_addr;
//    socketHandle = socket(addressInformation->ai_family, addressInformation->ai_socktype, 0);
//    if (socketHandle < 0)
//    {
//        ESP_LOGE(__func__, "    Failed to allocate socket.");
//        freeaddrinfo(addressInformation);
//        return;
//    }
//    if (connect(socketHandle, addressInformation->ai_addr, addressInformation->ai_addrlen) != 0)
//    {
//        ESP_LOGE(__func__, "    Socket connect failed errno: %d", errno);
//        close(socketHandle);
//        freeaddrinfo(addressInformation);
//        return;
//    }
//
//    freeaddrinfo(addressInformation);
//
//    if (write(socketHandle, request, strlen(request)) < 0)
//    {
//        ESP_LOGE(__func__, "    Socket send failed");
//        close(socketHandle);
//        return;
//    }
//
//    struct timeval timeOut;
//    timeOut.tv_sec = 5;
//    timeOut.tv_usec = 0;
//    if (setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &timeOut, sizeof(timeOut)) < 0)
//    {
//        ESP_LOGE(__func__, "    Failed to set socket receiving timeout");
//        close(socketHandle);
//        return;
//    }
//
//    do
//    {
//        bzero(receiveBuffer, sizeof(receiveBuffer));
//        r = read(socketHandle, receiveBuffer, sizeof(receiveBuffer) - 1);
//        for (int i = 0; i < r; i++)
//        {
//            putchar(receiveBuffer[i]);
//        }
//    }
//    while (r > 0);
//    printf("\n");
//
//    ESP_LOGI(__func__, "    Finished reading from socket. Last read return: %d errno: %d\r\n", r, errno);
//    close(socketHandle);
//
//    vPortFree(request);
//    _wifiResponse = pvPortMalloc(sizeof(struct ApplicationResponse));
//    _wifiResponse->interface = message->interface;
//    _wifiResponse->messageID = message->messageID;
//    _wifiResponse->payload = NULL;
//    _wifiResponse->payloadLength = 0;
//    SendResponse(ERR_COMPLETED_OK);
}
