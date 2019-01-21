/*
 *      WiFi.cpp
 *
 *      Implementation of the WiFi functionality.
 */
#include "WiFi.hpp"

/*
 *      Event handler for the WiFi loop.
 *
 *      This needs to be a C function prototype in order to be
 *      called from FreeRTOS.
 */
extern "C" esp_err_t WiFiEventHandler(void *ctx, system_event_t *event)
{
    WiFi *sender = (WiFi *) ctx;

    return(sender->EventHandler(ctx, event));
}

/*
 * ----------------------------------------------------------------------------
 *
 *                      Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Default constructor for the WiFi class.
 */
WiFi::WiFi()
{
    const size_t messageSize = sizeof(ApplicationRequest);
    SetQueueHandle(xQueueCreate(MAX_QUEUE_LENGTH, messageSize));
    if (GetQueueHandle() == NULL)
    {
        ESP_LOGI(__func__, "Cannot allocate WiFi processing queue.");
    }
}

/*
 *      Destructor for the WiFi class.
 *
 *      Free any resources used by the class.
 */
WiFi::~WiFi()
{

}

/*
 * ----------------------------------------------------------------------------
 *
 *                          Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Get the handle for the WiFi queue.
 */
QueueHandle_t WiFi::GetQueueHandle() const
{
    return QueueHandle;
}

/*
 *      Set the handle for the WiFi queue.
 */
void WiFi::SetQueueHandle(QueueHandle_t queueHandle)
{
    QueueHandle = queueHandle;
}

/*
 * ----------------------------------------------------------------------------
 *
 *                                 Methods
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Dispatch the WiFi request to the  appropriate method in the class.
 */
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

/*
 *      Setup the WiFi network.
 */
void WiFi::Setup()
{
    xWiFiEventGroup = xEventGroupCreate();
    xTaskCreate(WiFiTask, WIFI_TASK_NAME, 2048 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
}

/*
 *      Send the response and clean up the WiFi response object.
 *
 *      @param statusCode
 *          Response code to be sent back to the STM32
 */
void WiFi::SendResponse(uint32_t statusCode)
{
    _wifiResponse->code = statusCode;
    uart->QueueResponse(_wifiResponse);
    vPortFree(_wifiResponse);
    _wifiResponse = NULL;
}


/*
 *      Event handler for the WiFi system.
 *
 *      @param ctx
 *          Context (sender object) information.
 *      @param event
 *          Information relevant to the event being raised.
 */
esp_err_t WiFi::EventHandler(void *ctx, system_event_t *event)
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
                _wifiResponse->payload = (uint8_t *) pvPortMalloc(_wifiResponse->payloadLength);
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

/*
 *      Process the scan results.
 *
 *      @param event
 *          Event information passed to the event loop.
 */
void WiFi::ProcessAccessPointScanCompletion(system_event_t *event)
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
        _wifiResponse->payload = (uint8_t *) pvPortMalloc(_wifiResponse->payloadLength);
        Encoders::EncodeUInt16(apCount, _wifiResponse->payload);
        //
        //  Now we get the access point information.
        //
        wifi_ap_record_t *accessPoints = (wifi_ap_record_t *) pvPortMalloc(apCount * sizeof(wifi_ap_record_t));
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


/*
 *      Start the wireless hardware.
 *
 *      @param request
 *          Request sent from the STM32.
 */
void WiFi::StartNetwork(ApplicationRequest *message)
{
    ESP_LOGI(__func__, "Starting network.");
    if (_wifiResponse != NULL)
    {
        // TODO: Deal with this error.
        vPortFree(_wifiResponse);
    }
    _wifiResponse = (ApplicationResponse *) pvPortMalloc(sizeof(ApplicationResponse));
    _wifiResponse->interface = message->interface;
    _wifiResponse->messageID = message->messageID;
    _wifiResponse->payload = NULL;
    _wifiResponse->payloadLength = 0;
    _wifiResponse->code = 0;

    EventBits_t bits = xEventGroupGetBits(xWiFiEventGroup);
    if (!(bits & WIFI_INITIALISED_BIT))
    {
        char *ssid = (char *) message->payload;
        if ((ssid == NULL) || (strlen(ssid) > MaximumSSIDLength))
        {
            SendResponse(ERR_INVALID_SSID);
            return;
        }

        char *password = (char *) (message->payload + strlen(ssid) + 1);
        if (strlen(password) > MaximumPasswordLength)
        {
            SendResponse(ERR_INVALID_PASSWORD);
            return;
        }

        tcpip_adapter_init();
        ESP_ERROR_CHECK(esp_event_loop_init(WiFiEventHandler, this));

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config;
        wifi_config.sta.scan_method = DefaultScanMethod;
        wifi_config.sta.sort_method = DefaultSortMethod;
        wifi_config.sta.threshold.rssi = DefaultRSSI;
        wifi_config.sta.threshold.authmode = DefaultAuthorisationMode;
        memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
        if (strlen(ssid) < MaximumSSIDLength)
        {
            wifi_config.sta.ssid[strlen(ssid)] = 0;
        }
        memcpy(wifi_config.sta.password, password, strlen(password));
        if (strlen(ssid) < MaximumPasswordLength)
        {
            wifi_config.sta.password[strlen(password)] = 0;
        }
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(__func__, "Start called.");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        xEventGroupSetBits(xWiFiEventGroup, WIFI_INITIALISED_BIT | WIFI_CONNECTING_BIT);
        xEventGroupClearBits(xWiFiEventGroup, WIFI_READY_BIT);
    }
    else
    {
        ESP_LOGI(__func__, "Already started.");
        SendResponse(ERR_WIFI_ALREADY_STARTED);
    }
}

/*
 *      Get the full list of available networks.
 *
 *      @pre
 *          The network must be started before this method is called.
 *
 *      @param message
 *          Request sent from the STM32.
 */
void WiFi::GetAvailableNetworks(ApplicationRequest *request)
{
    ESP_LOGI(__func__, "Start");
    if (_wifiResponse != NULL)
    {
        ESP_LOGI(__func__, "Deleting old response structure");
        // TODO: Deal with this error.
        vPortFree(_wifiResponse);
    }
    _wifiResponse = (ApplicationResponse *) pvPortMalloc(sizeof(ApplicationResponse));
    _wifiResponse->interface = request->interface;
    _wifiResponse->messageID = request->messageID;
    _wifiResponse->payload = NULL;
    _wifiResponse->payloadLength = 0;
    _wifiResponse->code = 0;

    wifi_scan_config_t scanConf =
    {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));
    ESP_LOGI(__func__, "Scan in progress");
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
//    char *request = (char *) pvPortMalloc(maximumRequestLength);
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
