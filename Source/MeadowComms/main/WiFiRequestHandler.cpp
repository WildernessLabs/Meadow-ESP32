/*
 *  WiFiRequestHandler.cpp
 *
 *  Implementation of the WiFi functionality.
 */
#include "WiFiRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

/**
 *  Event handler for the WiFi loop.
 *
 *  This needs to be a C function prototype in order to be
 *  called from FreeRTOS.
 *
 *  @param context
 *      This is the context object set up when the event handler was first registered.
 *      In this application the context has been set to the instance of the calling class.
 *
 *  @param event
 *      Event information to be processed.
 */
extern "C" esp_err_t WiFiEventHandler(void *context, system_event_t *event)
{
    WiFiRequestHandler *sender = (WiFiRequestHandler *) context;

    return(sender->EventHandler(context, event));
}

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Determine if this class is instantiated already (i.e. this should be a singleton).
 */
bool WiFiRequestHandler::_instantiated = false;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Default constructor for the WiFi class.
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
}

/**
 *  Create a new object and set the message dispatcher accordingly.
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
 *  Destructor for the WiFi class.
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
 *  Get the currently selected antenna.
 *
 *  @param request
 *      Original request from the STM32.
 */
void WiFiRequestHandler::GetAntenna(Message *request) const
{
    ESP_LOGI(__func__, "Entering method.");
    Logging::DumpMessage(__func__, request);

    request->MessageType = MessageTypes::Response;
    Esp32Messaging::AntennaInfo *antenna = (Esp32Messaging::AntennaInfo *) pvPortMalloc(sizeof(Esp32Messaging::AntennaInfo));
    antenna->Antenna = (uint8_t) _currentAntenna;
    request->Payload = (uint8_t *) pvPortMalloc(Esp32Messaging::ANTENNA_INFO_ENCODED_MESSAGE_SIZE);
    Encoders::EncodeAntennaInfo(antenna, request->Payload, 0);
    vPortFree(antenna);
    request->PayloadLength = Esp32Messaging::ANTENNA_INFO_ENCODED_MESSAGE_SIZE;
    request->StatusCode = StatusCodes::CompletedOK;
    if (!_messageDispatcher->QueueMessageForStm32(request))
    {
        ESP_LOGE(__func__, "Cannot queue message for STM32.");
    }
}

/**
 *  Convert the antenna enum into a GPIO pin number
 *
 *  @param antenna
 *      Enum representing the antenna to be converted.
 *
 *  @returns
 *      GPIO pin number connected to the appropriate antenna.
 */
gpio_num_t WiFiRequestHandler::ConvertAntennaToPin(AntennaTypes::AntennaTypes antenna)
{
    return((antenna == AntennaTypes::OnBoard) ? ON_BOARD_ANTENNA_PIN : EXTERNAL_ANTENNA_PIN);
}

/**
 *  Switch the current antenna.
 *
 *  @param request
 *      Original request from the STM32.
 *
 *  TODO: Do we need to turn the WiFi off before switching the antenna?
 */
void WiFiRequestHandler::SetAntenna(Message *request)
{
    ESP_LOGI(__func__, "Entering method.");
    if (request->PayloadLength == Esp32Messaging::ANTENNA_INFO_ENCODED_MESSAGE_SIZE)
    {
        Esp32Messaging::AntennaInfo *antenna = Encoders::ExtractAntennaInfo(request->Payload);
        if (antenna->Antenna > (uint8_t ) AntennaTypes::Max)
        {
            request->StatusCode = StatusCodes::InvalidAntennaValue;
        }
        else
        {
            if (_currentAntenna != antenna->Antenna)
            {
                gpio_num_t newAntennaPin = ConvertAntennaToPin((AntennaTypes::AntennaTypes) antenna->Antenna);
                gpio_num_t oldAntennaPin = ConvertAntennaToPin(_currentAntenna);
                //
                //  Make sure the old antenna is switched off before the new one is turned on.
                //
                gpio_set_level(oldAntennaPin, 0);
                gpio_set_level(newAntennaPin, 1);
                _currentAntenna = (AntennaTypes::AntennaTypes) antenna->Antenna;
            }
            request->StatusCode = StatusCodes::CompletedOK;
        }
        vPortFree(antenna);
    }
    else
    {
        request->StatusCode = StatusCodes::InvalidAntennaData;
    }
    if (request->Payload)
    {
        vPortFree(request->Payload);
        request->Payload = nullptr;
    }
    request->PayloadLength = 0;
    request->MessageType = MessageTypes::Response;
    if (!_messageDispatcher->QueueMessageForStm32(request))
    {
        ESP_LOGE(__func__, "Cannot queue message for STM32.");
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Setup the WiFi network.
 */
void WiFiRequestHandler::Setup()
{
    RequestHandlerBase::Setup();
    /*
     *  Configure the antenna, by default, the on board antenna will be used.
     */
    //gpio_pad_select_gpio(EXTERNAL_ANTENNA);
    gpio_set_direction(EXTERNAL_ANTENNA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(EXTERNAL_ANTENNA_PIN, 0);
    //gpio_pad_select_gpio(ON_BOARD_ANTENNA);
    gpio_set_direction(ON_BOARD_ANTENNA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ON_BOARD_ANTENNA_PIN, 1);
    _currentAntenna = AntennaTypes::OnBoard;
    /*
     *  Now create the objects necessary for the WiFi tasks.
     */
    _xWiFiEventGroup = xEventGroupCreate();
    xTaskCreate(Task, _taskName, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
}

/**
 *  Dispatch the WiFi request to the appropriate method in the class.
 *
 *  @param request
 *      Data structure containing the request information from the sender.
 */
void WiFiRequestHandler::DispatchRequest(Message *request)
{
    switch (request->Function)
    {
        case WiFiFunction::Start:
            StartNetwork(request);
            break;
        case WiFiFunction::Stop:
            ESP_LOGE(__func__, "WiFi Stop function is not implemented yet.");
            break;
        case WiFiFunction::Connect:
            ESP_LOGE(__func__, "WiFi Connect function is not implemented yet.");
            break;
        case WiFiFunction::Disconnect:
            ESP_LOGE(__func__, "WiFi Disconnect function is not implemented yet.");
            break;
        case WiFiFunction::SetConfiguration:
            ESP_LOGE(__func__, "WiFi SetConfiguration function is not implemented yet.");
            break;
        case WiFiFunction::GetConfiguration:
            ESP_LOGE(__func__, "WiFi GetConfiguration function is not implemented yet.");
            break;
        case WiFiFunction::SetAntenna:
            SetAntenna(request);
            break;
        case WiFiFunction::GetAntenna:
            GetAntenna(request);
            break;
        case WiFiFunction::GetAccessPoints:
            GetAvailableNetworks(request);
            break;
        default:
            ESP_LOGI(__func__, "Unknown WiFi message received, function %d", request->Function);
            if (request->Payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(__func__, request->Payload, request->PayloadLength, ESP_LOG_INFO);
            }
            break;
    }
    //
    //  Memory for the payload is allocated on the heap and it needs to be released.
    //
//    if (request->Payload != NULL)
//    {
//        vPortFree(request->Payload);
//    }
}

/**
 *  Event handler for the WiFi system.
 *
 *  @param ctx
 *      Context (sender object) information.
 *  @param event
 *      Information relevant to the event being raised.
 */
esp_err_t WiFiRequestHandler::EventHandler(void *ctx, system_event_t *event)
{
    EventBits_t bits;

    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            _currentRetryCount = 0;
            bits = xEventGroupGetBits(_xWiFiEventGroup);
            //
            //  TODO: We can sometimes get here when _wifiResponse is NULL, need to review this.
            //
            if ((_wifiResponse != NULL) && (bits & WIFI_CONNECTING_BIT))
            {
                _wifiResponse->PayloadLength = sizeof(ip4_addr_t);
                _wifiResponse->Payload = (uint8_t *) pvPortMalloc(_wifiResponse->PayloadLength);
                memcpy(_wifiResponse->Payload, &event->event_info.got_ip.ip_info.ip, _wifiResponse->PayloadLength);
                SendResponse((uint32_t) StatusCodes::CompletedOK);
            }
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_READY_BIT);
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
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
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            ProcessAccessPointScanCompletion(event);
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 *  Send the response and clean up the WiFi response object.
 *
 *  @param statusCode
 *      Response code to be sent back to the STM32
 */
void WiFiRequestHandler::SendResponse(uint32_t statusCode)
{
    _wifiResponse->StatusCode = statusCode;
    //
    //  TODO: Fix the requirement for a UART in the WiFi handler.
    //
//    uart->QueueResponse(_wifiResponse);
    vPortFree(_wifiResponse);
    _wifiResponse = NULL;
}

/**
 *  Start the wireless hardware.
 *
 *  @param message
 *      Request sent from the STM32.
 */
void WiFiRequestHandler::StartNetwork(Message *message)
{
    ESP_LOGI(__func__, "Starting network.");
    if (_wifiResponse != nullptr)
    {
        // TODO: Deal with this error.
        vPortFree(_wifiResponse);
    }
    _wifiResponse = (Message *) pvPortMalloc(sizeof(Message));
    _wifiResponse->Interface = message->Interface;
    _wifiResponse->MessageID = message->MessageID;
    _wifiResponse->Payload = nullptr;
    _wifiResponse->PayloadLength = 0;
    _wifiResponse->StatusCode = 0;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_INITIALISED_BIT))
    {
        Esp32Messaging::WiFiCredentials *credentials = Encoders::ExtractWiFiCredentials(message->Payload);
        uint32_t networkNameLength = 0;
        if ((credentials->NetworkName == nullptr) || (strlen(credentials->NetworkName) > MAXIMUM_SSID_LENGTH))
        {
            SendResponse(StatusCodes::InvalidSSID);
            return;
        }
        else
        {
            networkNameLength = strlen(credentials->NetworkName);
        }
        uint32_t passwordLength = strlen(credentials->Password);
        if (strlen(credentials->Password) > MAXIMUM_PASSWORD_LENGTH)
        {
            SendResponse(StatusCodes::InvalidPassword);
            return;
        }

        tcpip_adapter_init();
        //
        //  The second parameter in the call to esp_event_loop_init is set to 'this'
        //  and will be passed as the context parameter to the WiFiEventHandler
        //  event handler.
        //
        ESP_ERROR_CHECK(esp_event_loop_init(WiFiEventHandler, this));
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config;
        wifi_config.sta.scan_method = DEFAULT_SCAN_METHOD;
        wifi_config.sta.sort_method = DEFAULT_SORT_METHOD;
        wifi_config.sta.threshold.rssi = DEFAULT_RSSI;
        wifi_config.sta.threshold.authmode = DEFAULT_AUTH_MODE;
        memcpy((void *) wifi_config.sta.ssid, credentials->NetworkName, networkNameLength);
        if (networkNameLength < MAXIMUM_SSID_LENGTH)
        {
            wifi_config.sta.ssid[networkNameLength] = 0;
        }
        memcpy((void *) wifi_config.sta.password, credentials->Password, passwordLength);
        if (passwordLength < MAXIMUM_PASSWORD_LENGTH)
        {
            wifi_config.sta.password[passwordLength] = 0;
        }
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_INITIALISED_BIT | WIFI_CONNECTING_BIT);
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_BIT);
        ESP_LOGI(__func__, "Network started.");
    }
    else
    {
        SendResponse(StatusCodes::WiFiAlreadyStarted);
    }
}

/**
 *  Process the scan results.
 *
 *  @param event
 *      Event information passed to the event loop.
 */
void WiFiRequestHandler::ProcessAccessPointScanCompletion(system_event_t *event)
{
    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);
    ESP_LOGI(__func__, "Number of access points found: %d\n", apCount);
    if (apCount == 0)
    {
        _wifiResponse->Payload = NULL;
        _wifiResponse->PayloadLength = 0;
    }
    else
    {
        //
        //  First two bytes in this payload are the number of access points.
        //
        _wifiResponse->PayloadLength = (ACCESS_POINT_INFORMATION_LENGTH * apCount) + 2;
        _wifiResponse->Payload = (uint8_t *) pvPortMalloc(_wifiResponse->PayloadLength);
        Encoders::EncodeUInt16(apCount, _wifiResponse->Payload);
        //
        //  Now we get the access point information.
        //
        wifi_ap_record_t *accessPoints = (wifi_ap_record_t *) pvPortMalloc(apCount * sizeof(wifi_ap_record_t));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, accessPoints));
        uint8_t *destination = _wifiResponse->Payload + 2;
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
    SendResponse(StatusCodes::CompletedOK);
}

/**
 *  Get the full list of available networks.
 *
 *  @pre
 *      The network must be started before this method is called.
 *
 *  @param request
 *      Request sent from the STM32.
 */
void WiFiRequestHandler::GetAvailableNetworks(Message *request)
{
    if (_wifiResponse != NULL)
    {
        ESP_LOGI(__func__, "Deleting old response structure");
        // TODO: Deal with this error.
        vPortFree(_wifiResponse);
    }
    _wifiResponse = (Message *) pvPortMalloc(sizeof(Message));
    _wifiResponse->Interface = request->Interface;
    _wifiResponse->MessageID = request->MessageID;
    _wifiResponse->Payload = NULL;
    _wifiResponse->PayloadLength = 0;
    _wifiResponse->StatusCode = 0;

    wifi_scan_config_t scanConf =
    {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));
}

void WiFiRequestHandler::HttpGet(Message *message)
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
