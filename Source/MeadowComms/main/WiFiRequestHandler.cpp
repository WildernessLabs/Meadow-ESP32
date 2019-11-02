/*
 *  WiFiRequestHandler.cpp
 *
 *  Implementation of the WiFi functionality.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WiFiRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

//extern "C" char* utoa(int a, char* buffer, unsigned char radix);

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
 *  This method will reuse the request object and use this as the message to send the
 *  result back to the caller.
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
    int encodedBufferLength = Encoders::EncodedAntennaInfoBufferSize(antenna);
    request->Payload = (uint8_t *) pvPortMalloc(encodedBufferLength);
    Encoders::EncodeAntennaInfo(antenna, request->Payload);
    vPortFree(antenna);
    request->PayloadLength = encodedBufferLength;
    request->StatusCode = StatusCodes::CompletedOK;
    if (!_messageDispatcher->QueueMessageForStm32(request))
    {
        ESP_LOGE(__func__, "Cannot queue message for STM32.");
    }
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
}

/**
 *  Switch the current antenna.
 *
 *  This method will reuse the original request object to return the result to the caller.
 *
 *  The memory holding the original payload will be released and the payload set to nullptr
 *  with a 0 byte length.
 *
 *  @param request
 *      Original request from the STM32.
 *
 *  TODO: Do we need to turn the WiFi off before switching the antenna?
 */
void WiFiRequestHandler::SetAntenna(Message *request)
{
    ESP_LOGI(__func__, "Entering method.");
    if (request->PayloadLength > 0)
    {
        Esp32Messaging::AntennaInfo *antenna = Encoders::ExtractAntennaInfo(request->Payload);
        if (antenna->Antenna > (uint8_t) AntennaTypes::Max)
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
    request->DeletePayload();
    request->MessageType = MessageTypes::Response;
    if (!_messageDispatcher->QueueMessageForStm32(request))
    {
        ESP_LOGE(__func__, "Cannot queue message for STM32.");
    }
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

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
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
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
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_BIT);
    _currentRequest = nullptr;
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
        case WiFiFunction::HttpGet:
            HttpGet(request);
            break;
        default:
            ESP_LOGI(__func__, "Unknown WiFi message received, function %d", request->Function);
            if (request->Payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(__func__, request->Payload, request->PayloadLength, ESP_LOG_INFO);
            }
            break;
    }
    xEventGroupWaitBits(_xWiFiEventGroup, WIFI_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
}

/**
 *  Send the response and clean up the WiFi request object.
 *
 *  @param request
 *      Message containing the request being processed.
 *
 *  @param statusCode
 *      Response code to be sent back to the STM32
 */
void WiFiRequestHandler::SendResponse(Message *request, uint32_t statusCode)
{
    request->DeletePayload();
    request->StatusCode = statusCode;
    _messageDispatcher->QueueMessageForStm32(request);
    _currentRequest = nullptr;
}

/**
 *  Start the wireless hardware.
 *
 *  @param request
 *      Request sent from the STM32.
 */
void WiFiRequestHandler::StartNetwork(Message *request)
{
    ESP_LOGI(__func__, "Starting network.");

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_INITIALISED_BIT))
    {
        Esp32Messaging::WiFiCredentials *credentials = Encoders::ExtractWiFiCredentials(request->Payload);
        uint32_t networkNameLength = 0;
        if ((credentials->NetworkName == nullptr) || (strlen(credentials->NetworkName) > MAXIMUM_SSID_LENGTH))
        {
            SendResponse(request, StatusCodes::InvalidSSID);
            return;
        }
        else
        {
            networkNameLength = strlen(credentials->NetworkName);
        }
        uint32_t passwordLength = strlen(credentials->Password);
        if (strlen(credentials->Password) > MAXIMUM_PASSWORD_LENGTH)
        {
            SendResponse(request, StatusCodes::InvalidPassword);
            return;
        }
        _currentRequest = request;
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
        tcpip_adapter_init();
        //
        //  The second parameter in the call to esp_event_loop_init is set to 'this'
        //  and will be passed as the context parameter to the WiFiEventHandler
        //  event handler.
        //
        ESP_ERROR_CHECK(esp_event_loop_init(WiFiEventHandler, this));
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        if (_wifiConfig)
        {
            vPortFree(_wifiConfig);
        }
        _wifiConfig = (wifi_config_t *) pvPortMalloc(sizeof(wifi_config_t));
        bzero(_wifiConfig, sizeof(wifi_config_t));
        memset(_wifiConfig, 0, sizeof(wifi_config_t));
        _wifiConfig->sta.scan_method = DEFAULT_SCAN_METHOD;
        _wifiConfig->sta.sort_method = DEFAULT_SORT_METHOD;
        _wifiConfig->sta.threshold.rssi = DEFAULT_RSSI;
        _wifiConfig->sta.threshold.authmode = DEFAULT_AUTH_MODE;
        //
        //  Don't use strcpy for the following two copies in case the SSID
        //  or the password are the maximum length as the terminating 0 could
        //  corrupt the byte following the appropriate array.
        //
        memcpy((void *) _wifiConfig->sta.ssid, credentials->NetworkName, networkNameLength);
        if (networkNameLength < MAXIMUM_SSID_LENGTH)
        {
            _wifiConfig->sta.ssid[networkNameLength] = 0;
        }
        memcpy((void *) _wifiConfig->sta.password, credentials->Password, passwordLength);
        if (passwordLength < MAXIMUM_PASSWORD_LENGTH)
        {
            _wifiConfig->sta.password[passwordLength] = 0;
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, _wifiConfig));
        ESP_ERROR_CHECK(esp_wifi_start());
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_INITIALISED_BIT | WIFI_CONNECTING_BIT);
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_BIT);
        ESP_LOGI(__func__, "Network started.");
    }
    else
    {
        SendResponse(request, StatusCodes::WiFiAlreadyStarted);
    }
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
    wifi_scan_config_t scanConf =
    {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };
    _currentRequest = request;
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));
}

void WiFiRequestHandler::HttpGet(Message *message)
{
    struct addrinfo hints;
    bzero((void *) &hints, sizeof(addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *addressInformation;
    struct in_addr *address;
    int socketHandle, r;

    Esp32Messaging::HttpGetRequest *httpRequest = Encoders::ExtractHttpGetRequest(message->Payload);
    char portNumber[6];
    sprintf(portNumber, "%d", httpRequest->PortNumber);

    int maximumRequestLength = strlen(httpRequest->Uri) +strlen(httpRequest->ServerName) + 60;
    char *request = (char *) pvPortMalloc(maximumRequestLength);
    int actualRequestLength = snprintf(request, maximumRequestLength,  "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: esp-idf/3.3 esp32\r\n\r\n", httpRequest->Uri, httpRequest->ServerName);
    if (actualRequestLength > (maximumRequestLength - 1))
    {
        ESP_LOGI(__func__, "Actual request length, %d,  would be too long for the allocated buffer %d.", actualRequestLength, actualRequestLength);
    }
    ESP_LOGI(__func__, "HTTP Get Request");
    ESP_LOGI(__func__, "    Host: %s", httpRequest->ServerName);
    ESP_LOGI(__func__, "    URI: %s", httpRequest->Uri);
    ESP_LOGI(__func__, "    Request length: %d", strlen(request));
    ESP_LOGI(__func__, "    Request: %s", request);

    int errorCode = getaddrinfo(httpRequest->ServerName, portNumber, &hints, &addressInformation);

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

    bool firstSent = false;
    int blockSize = 200;
    uint32_t sequenceNumber = 0;
    char *receiveBuffer = (char *) pvPortMalloc(blockSize);
    do
    {
        bzero(receiveBuffer, blockSize);
        r = read(socketHandle, receiveBuffer, blockSize - 1);

        if (!firstSent || (r == 0))
        {
            firstSent = true;
            Message *response = message->CreateCopyOnHeap(false);
            response->MessageType = MessageTypes::Response;

            Esp32Messaging::HttpGetResponse *responseData = (Esp32Messaging::HttpGetResponse *) pvPortMalloc(sizeof(Esp32Messaging::HttpGetResponse));
            responseData->SequenceNumber = sequenceNumber;
            responseData->ResponseLength = r;
            if (r > 0)
            {
                responseData->Response = (uint8_t *) pvPortMalloc(r);
                memcpy(responseData->Response, receiveBuffer, r);
            }
            else
            {
                responseData->Response = nullptr;
            }
            response->StatusCode = StatusCodes::CompletedOK;
            response->PayloadLength = Encoders::EncodedHttpGetResponseBufferSize(responseData);
            response->Payload = (uint8_t *) pvPortMalloc(response->PayloadLength);
            Encoders::EncodeHttpGetResponse(responseData, response->Payload);
            vPortFree(responseData->Response);
            vPortFree(responseData);
            sequenceNumber++;
            _messageDispatcher->QueueMessageForStm32(response);
        }
    }
    while (r > 0);
    vPortFree(receiveBuffer);

    ESP_LOGI(__func__, "    Finished reading from socket. Last read return: %d errno: %d\r\n", r, errno);
    close(socketHandle);

    vPortFree(request);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Event Handlers
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  Event handler for the WiFi system.
 *
 *  @param ctx
 *      Context (sender object) information.
 *
 *  @param event
 *      Information relevant to the event being raised.
 *
 *  @returns
 *      Always returns a success code and problems are dealt with by
 *      retrying or sending messages to the STM32.
 */
esp_err_t WiFiRequestHandler::EventHandler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            StationGotIpEvent(event);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            StationDisconnectedEvent(event);
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            ScanCompleteEvent(event);
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 *  The access point has just given the ESP32 an IP address.
 *
 *  @param eventData
 *      Data for the Got IP event.
 */
void WiFiRequestHandler::StationGotIpEvent(system_event_t *eventData)
{
    _currentRetryCount = 0;
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (bits & WIFI_CONNECTING_BIT)
    {
        _currentRequest->DeletePayload();
        _currentRequest->PayloadLength = sizeof(ip4_addr_t);
        _currentRequest->Payload = (uint8_t *) pvPortMalloc(_currentRequest->PayloadLength);
        memcpy(_currentRequest->Payload, &eventData->event_info.got_ip.ip_info.ip, _currentRequest->PayloadLength);
        _currentRequest->MessageType = MessageTypes::Response;
        _currentRequest->StatusCode = StatusCodes::CompletedOK;
        _messageDispatcher->QueueMessageForStm32(_currentRequest);
        _currentRequest = nullptr;
    }
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_READY_BIT);
}

/**
 *  Process the WiFi disconnected event.
 *
 *  @param eventData
 *      Data for the Got IP event.
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

/**
 *  Process the scan results.
 *
 *  @param event
 *      Event information passed to the event loop.
 */
void WiFiRequestHandler::ScanCompleteEvent(system_event_t *event)
{
    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);

    ESP_LOGI(__func__, "Number of access points found: %d\n", apCount);
    _currentRequest->DeletePayload();
    _currentRequest->MessageType = MessageTypes::Response;
    if (apCount == 0)
    {
        _currentRequest->Payload =  nullptr;
        _currentRequest->PayloadLength = 0;
        _currentRequest->StatusCode = StatusCodes::CompletedOK;
    }
    else
    {
        Esp32Messaging::AccessPointList *accessPointList = (Esp32Messaging::AccessPointList *) pvPortMalloc(sizeof(Esp32Messaging::AccessPointList));
        accessPointList->NumberOfAccessPoints = apCount;
        //
        //  Now we get the access point information.
        //
        wifi_ap_record_t *accessPoints = (wifi_ap_record_t *) pvPortMalloc(apCount * sizeof(wifi_ap_record_t));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, accessPoints));

        Esp32Messaging::AccessPoint *accessPoint = (Esp32Messaging::AccessPoint *) pvPortMalloc(sizeof(Esp32Messaging::AccessPoint));
        int singleAccessPointSize = Encoders::EncodedAccessPointBufferSize(accessPoint);
        accessPointList->AccessPointsLength = singleAccessPointSize * apCount;
        accessPointList->AccessPoints = (uint8_t *) pvPortMalloc(accessPointList->AccessPointsLength);
        uint8_t *currentLocation = accessPointList->AccessPoints;
        for (int index = 0; index < apCount; index++)
        {
            bzero(accessPoint, sizeof(Esp32Messaging::AccessPoint));
            memcpy(accessPoint->Ssid, &accessPoints[index].ssid, 33);
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
        _currentRequest->StatusCode = StatusCodes::CompletedOK;
        _currentRequest->PayloadLength = Encoders::EncodedAccessPointListBufferSize(accessPointList);
        _currentRequest->Payload = (uint8_t *) pvPortMalloc(_currentRequest->PayloadLength);
        Encoders::EncodeAccessPointList(accessPointList, _currentRequest->Payload);
        vPortFree(accessPointList);
    }
    _messageDispatcher->QueueMessageForStm32(_currentRequest);
    _currentRequest = nullptr;
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
}
