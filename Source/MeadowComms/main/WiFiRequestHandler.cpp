/*
 *  WiFiRequestHandler.cpp
 *
 *  Implementation of the WiFi functionality.
 */
#include "driver/gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "Mapping.hpp"
#include "DebugSettings.hpp"

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
    WiFiRequestHandler *sender = static_cast<WiFiRequestHandler *>(context);

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
    //
    //  Get configuration from NVS.
    //
    NvsManager::ErrorCodes result = NvsManager::ReadBlob(CONFIG_NAME, &_config, sizeof(_config));
    if (result != NvsManager::ErrorCodes::Ok)
    {
        memcpy(static_cast<void *>(&_config), &DEFAULT_WIFI_CONFIGURATION, sizeof(DEFAULT_WIFI_CONFIGURATION));
        if ((result == NvsManager::ErrorCodes::BufferSizeMismatch) || (result == NvsManager::ErrorCodes::NotFound))
        {
            NvsManager::WriteBlob(CONFIG_NAME, &_config, sizeof(_config));
        }
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
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    Logging::DumpMessage(__func__, request);

    request->MessageType = MessageTypes::Response;
    Esp32Messaging::AntennaInfo *antenna = static_cast<Esp32Messaging::AntennaInfo *>(pvPortMalloc(sizeof(Esp32Messaging::AntennaInfo)));
    antenna->Antenna = static_cast<uint8_t>(_config.Antenna);
    int encodedBufferLength = Encoders::EncodedAntennaInfoBufferSize(antenna);
    request->Payload = static_cast<uint8_t *>(pvPortMalloc(encodedBufferLength));
    Encoders::EncodeAntennaInfo(antenna, request->Payload);
    vPortFree(antenna);
    request->PayloadLength = encodedBufferLength;
    request->StatusCode = StatusCodes::CompletedOk;
    if (!_messageDispatcher->QueueMessageForStm32(request))
    {
        ESP_LOGE(__func__, "Cannot queue message for STM32.");
    }
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
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
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    if (request->PayloadLength > 0)
    {
        Esp32Messaging::AntennaInfo *antenna = Encoders::ExtractAntennaInfo(request->Payload);
        if (antenna->Antenna > static_cast<uint8_t>(AntennaTypes::Max))
        {
            request->StatusCode = StatusCodes::InvalidAntennaValue;
        }
        else
        {
            if (_config.Antenna != antenna->Antenna)
            {
                SwitchAntenna((AntennaTypes::AntennaTypes) antenna->Antenna);
            }
            request->StatusCode = StatusCodes::CompletedOk;
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

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
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
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    return((antenna == AntennaTypes::OnBoard) ? ON_BOARD_ANTENNA_PIN : EXTERNAL_ANTENNA_PIN);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Change the current antenna and actrivate it.
 *
 *  @param newAntenna
 *      New antenna to use.
 */
void WiFiRequestHandler::SwitchAntenna(AntennaTypes::AntennaTypes newAntenna)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    //
    // TODO: What happens to any pending requests and do we disconnect WiFi first?
    //
    gpio_num_t newAntennaPin = ConvertAntennaToPin(newAntenna);
    gpio_num_t oldAntennaPin = ConvertAntennaToPin(static_cast<AntennaTypes::AntennaTypes>(_config.Antenna));
    //
    //  Make sure the old antenna is switched off before the new one is turned on.
    //
    gpio_set_level(oldAntennaPin, 0);
    gpio_set_level(newAntennaPin, 1);
    _config.Antenna = newAntenna;

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  Setup the WiFi network.
 */
void WiFiRequestHandler::Setup()
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    RequestHandlerBase::Setup();
    gpio_set_direction(EXTERNAL_ANTENNA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(EXTERNAL_ANTENNA_PIN, 0);
    gpio_set_direction(ON_BOARD_ANTENNA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ON_BOARD_ANTENNA_PIN, 1);

    if (_config.Antenna > static_cast<uint8_t>(AntennaTypes::Max))
    {
        _config.Antenna = AntennaTypes::OnBoard;
    }
    SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(_config.Antenna));
    /*
     *  Now create the objects necessary for the WiFi tasks.
     */
    _xWiFiEventGroup = xEventGroupCreate();
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);
    xTaskCreate(Task, TASK_NAME, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  Dispatch the WiFi request to the appropriate method in the class.
 *
 *  @param request
 *      Data structure containing the request information from the sender.
 */
void WiFiRequestHandler::DispatchRequest(Message *request)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_BIT);
    _currentRequest = nullptr;
    switch (request->Function)
    {
        case WiFiFunction::Start:
            StartNetwork(request);
            break;
        case WiFiFunction::Stop:
            Stop(request);
            break;
        case WiFiFunction::ConnectToAp:
            ConnectToAp(request);
            break;
        case WiFiFunction::Disconnect:
            Disconnect(request);
            break;
        case WiFiFunction::SetConfiguration:
            SetConfiguration(request);
            break;
        case WiFiFunction::GetConfiguration:
            GetConfiguration(request);
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
        case WiFiFunction::Close:
            Close(request);
            break;
        case WiFiFunction::Read:
            Read(request);
            break;
        case WiFiFunction::SetSockOpt:
            SetSockOpt(request);
            break;
        case WiFiFunction::Write:
            Write(request);
            break;
        case WiFiFunction::FreeAddrInfo:
            FreeAddrInfo(request);
            break;
        case WiFiFunction::Connect:
            Connect(request);
            break;
        case WiFiFunction::Socket:
            Socket(request);
            break;
        case WiFiFunction::GetAddrInfo:
            GetAddrInfo(request);
            break;
        case WiFiFunction::SendTo:
            SendTo(request);
            break;
        case WiFiFunction::RecvFrom:
            RecvFrom(request);
            break;
        case WiFiFunction::Poll:
            Poll(request);
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

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
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
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    request->DeletePayload();
    request->StatusCode = statusCode;
    _messageDispatcher->QueueMessageForStm32(request);
    _currentRequest = nullptr;

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Get the configuration for the WiFi subsystem.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::GetConfiguration(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    // TODO: WiFi - GetConfiguration
    ESP_LOGE(__func__, "GetConfiguration function is not implemented yet.");

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Set the configuration for the WiFi subsystem.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::SetConfiguration(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    // TODO: WiFi - SetConfiguration
    ESP_LOGE(__func__, "SetConfiguration function is not implemented yet.");

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Disconnect from any access points.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::Disconnect(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    // TODO: WiFi - Disconnect from access point.
    ESP_LOGE(__func__, "Disconnect function is not implemented yet.");

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Stop the network and release all resources associated with any active connections.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::Stop(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    // TODO: WiFi - Stop network
    ESP_LOGE(__func__, "Stop function is not implemented yet.");

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Connect to a specified access point.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ConnectToAp(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    // TODO: WiFi - Connect to access point.
    ESP_LOGE(__func__, "Connect to access point function is not implemented yet.");

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  Start the wireless hardware.
 *
 *  @param request
 *      Request sent from the STM32.
 */
void WiFiRequestHandler::StartNetwork(Message *request)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_INITIALISED_BIT))
    {
        Esp32Messaging::WiFiCredentials *credentials = Encoders::ExtractWiFiCredentials(request->Payload);
        uint32_t networkNameLength = 0;
        if ((credentials->NetworkName == nullptr) || (strlen(credentials->NetworkName) > MAXIMUM_SSID_LENGTH))
        {
            SendResponse(request, StatusCodes::InvalidSsid);
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
        _wifiConfig = static_cast<wifi_config_t *>(pvPortMalloc(sizeof(wifi_config_t)));
        bzero(_wifiConfig, sizeof(wifi_config_t));
        _wifiConfig->sta.scan_method = DEFAULT_SCAN_METHOD;
        _wifiConfig->sta.sort_method = DEFAULT_SORT_METHOD;
        _wifiConfig->sta.threshold.rssi = DEFAULT_RSSI;
        _wifiConfig->sta.threshold.authmode = DEFAULT_AUTH_MODE;
        //
        //  Don't use strcpy for the following two copies in case the SSID
        //  or the password are the maximum length as the terminating 0 could
        //  corrupt the byte following the appropriate array.
        //
        memcpy(static_cast<void *>(_wifiConfig->sta.ssid), credentials->NetworkName, networkNameLength);
        if (networkNameLength < MAXIMUM_SSID_LENGTH)
        {
            _wifiConfig->sta.ssid[networkNameLength] = 0;
        }
        memcpy(static_cast<void *>(_wifiConfig->sta.password), credentials->Password, passwordLength);
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

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
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
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    wifi_active_scan_time_t activeScanTime =
    {
        .min = 500,
        .max = 1500
    };
    wifi_scan_time_t scanTime =
    {
        .active = activeScanTime
    };
    wifi_scan_config_t scanConf =
    {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = scanTime
    };
    _currentRequest = request;
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, true));

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the getaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getaddrinfo method.
 */
void WiFiRequestHandler::GetAddrInfo(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    Esp32Messaging::GetAddrInfoRequest *request = Encoders::ExtractGetAddrInfoRequest(message->Payload);
    //
    Esp32Messaging::AddrInfo *extractedHints = Encoders::ExtractAddrInfo(request->Hints);
    struct addrinfo hints;
    bzero(static_cast<void *>(&hints), sizeof(addrinfo));
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
        Esp32Messaging::SockAddr sa;
        Esp32Messaging::AddrInfo a;

        sa.Family = addressInformation->ai_addr->sa_family;
        sa.Length = addressInformation->ai_addr->sa_len;
        //
        //  We can perform a memcpy here as sa.Data is a fixed byte array
        //  in the structure so we do not need to allocate the memory first.
        //
        memcpy(sa.Data, addressInformation->ai_addr->sa_data, addressInformation->ai_addr->sa_len);
        a.AddrLength = Encoders::EncodedSockAddrBufferSize(&sa);
        a.Addr = static_cast<uint8_t *>(pvPortMalloc(a.AddrLength));

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
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the socket method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the socket method.
 */
void WiFiRequestHandler::Socket(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    Esp32Messaging::SocketRequest *request = Encoders::ExtractSocketRequest(message->Payload);

    Esp32Messaging::IntegerResponse response;
    response.Result = socket(request->Domain, request->Type, request->Protocol);

#if defined(WIFI_FULL_LOGGING) || defined(WIFI_SOCKET)
    ESP_LOGI(__func__, "Created socket %d", response.Result);
#endif

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the connect method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the connect method.
 */
void WiFiRequestHandler::Connect(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    Esp32Messaging::ConnectRequest *request = Encoders::ExtractConnectRequest(message->Payload);
    Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);

#if defined(WIFI_FULL_LOGGING) || defined(WIFI_CONNECT)
    ESP_LOGI(__func__, "Connecting socket: %d", request->SocketHandle);
#endif

    struct sockaddr sa;
    bzero(&sa, sizeof(struct sockaddr));
    int family;
    Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
    sa.sa_family = (uint8_t) (family & 0xff);
    sa.sa_len = socketAddress->Length;
    memcpy(sa.sa_data, socketAddress->Data, 14);

    struct sockaddr_in *ain = (struct sockaddr_in *) &sa;
    ESP_LOGI(__func__, "IP address: %s", inet_ntoa(ain->sin_addr));

    Esp32Messaging::IntegerAndErrnoResponse response;
    response.Result = 0;
    response.Result = connect(request->SocketHandle, &sa, request->AddrLen);
    if (response.Result != 0)
    {
        ESP_LOGI(__func__, "Error code: %s", strerror(errno));
        int32_t result = 0;
        if (Mapping::GetStmValue(Mapping::Errno, errno, &result) != 0)
        {
            result = Mapping::GetStmValue(Mapping::Errno, EFAULT, &result);
        }
        response.ResponseErrno = result;
        response.Result = -1;
    }

#if defined(WIFI_FULL_LOGGING) || defined(WIFI_CONNECT)
    ESP_LOGI(__func__, "Result (errno): %d", response.Result);
#endif

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the freeaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the freeaddrinfo method.
 */
void WiFiRequestHandler::FreeAddrInfo(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    ESP_LOGI(__func__, "Releasing addrinfo storage.");
    Esp32Messaging::FreeAddrInfoRequest *request = Encoders::ExtractFreeAddrInfoRequest(message->Payload);

    freeaddrinfo(reinterpret_cast<struct addrinfo *>(request->AddrInfoAddress));

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = 0;
    message->Payload = nullptr;
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;

    vTaskDelay(40 / portTICK_PERIOD_MS);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the setsockopt method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the setsockopt method.
 */
void WiFiRequestHandler::SetSockOpt(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    Esp32Messaging::SetSockOptRequest *request = Encoders::ExtractSetSockOptRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response;

    void *optionValue = nullptr;
    int optionLength = 0;
    Esp32Messaging::TimeVal *tv = nullptr;
    struct timeval timeOut;

    int32_t optionName;
    Mapping::GetEspValue(Mapping::SocketOptions, request->OptionName, &optionName);
    switch (optionName)
    {
        // TODO: Implement remaining setsockopt features.
        case SO_ACCEPTCONN:     /* Socket has had listen() */
            break;
        case SO_LINGER:         /* Linger on close if data present */
            break;
        case SO_DONTLINGER:     /* Don't linger on close if data present */
            break;
        case SO_RCVBUF:         /* Set the receive buffer size */
            break;
        case SO_SNDTIMEO:       /* Set the send timeout */
            break;
        case SO_RCVTIMEO:       /* Set the receive timeout */
            tv = Encoders::ExtractTimeVal(request->OptionValue);
            timeOut.tv_sec = tv->TvSec;
            timeOut.tv_usec = tv->TvUsec;
            vPortFree(tv);
            optionValue = &timeOut;
            optionLength = sizeof(timeOut);
            break;
        case SO_ERROR:          /* Get error status and clear */
            break;
        case SO_REUSEADDR:
            optionValue = request->OptionValue;
            optionLength = request->OptionValueLength;
            break;
        case SO_TYPE:           /* Get socket type */
            break;
        case SO_NO_CHECK:       /* Don't create UDP checksum */
            break;
    }

    int32_t level = 0;
    Mapping::GetEspValue(Mapping::SocketLevel, request->Level, &level);
    response.Result = setsockopt(request->SocketNumber, level, optionName, optionValue, optionLength);
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
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the write method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the write method.
 */
void WiFiRequestHandler::Write(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

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
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the read method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the read method.
 */
void WiFiRequestHandler::Read(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

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
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the close method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getaddrinfo method.
 */
void WiFiRequestHandler::Close(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

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
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the sendto method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the sendto method.
 */
void WiFiRequestHandler::SendTo(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    Esp32Messaging::SendToRequest *request = Encoders::ExtractSendToRequest(message->Payload);

    Esp32Messaging::IntegerAndErrnoResponse response;
    int32_t mappedFlags = 0;
    response.ResponseErrno = 0;
    response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
    if (response.Result == 0)
    {
        Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->DestinationAddress);
        struct sockaddr sa;
        bzero(&sa, sizeof(struct sockaddr));
        int32_t family = 0;
        Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
        sa.sa_family = (unsigned char) (family & 0xff);
        sa.sa_len = socketAddress->Length;
        memcpy(sa.sa_data, socketAddress->Data, 14);
        vPortFree(socketAddress);
        
        response.Result = sendto(request->SocketHandle, request->Buffer, request->Length, mappedFlags, &sa, request->DestinationAddressLen);
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

#if defined(WIFI_DEBUG_SENDTO_DATA) || defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Sent %d bytes.", request->Length);
    ESP_LOG_BUFFER_HEX(__func__, request->Buffer, request->Length);
    ESP_LOGI(__func__, "Result: %d, errno: %d", response.Result, response.ResponseErrno);
#endif

    vPortFree(request->Buffer);
    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the recvfrom method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the recvfrom method.
 */
void WiFiRequestHandler::RecvFrom(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    Esp32Messaging::RecvFromRequest *request = Encoders::ExtractRecvFromRequest(message->Payload);

    Esp32Messaging::RecvFromResponse response;
    int32_t mappedFlags = 0;
    response.ResponseErrno = 0;
    response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
    if (response.Result == 0)
    {
        struct sockaddr *sa = NULL;
        socklen_t len;
        if (request->GetSourceAddress != 0)
        {
            sa = (struct sockaddr *) pvPortMalloc(sizeof(struct sockaddr));
            bzero(sa, sizeof(struct sockaddr));
            len = sizeof(struct sockaddr);
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
            response.Result = recvfrom(request->SocketHandle, response.Buffer, request->Length, mappedFlags, sa, (sa == NULL) ? NULL : &len);
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
                Esp32Messaging::SockAddr socketAddress;
                bzero((void *) &socketAddress, sizeof(Esp32Messaging::SockAddr));
                int family;
                Mapping::GetStmValue(Mapping::AddressFamily, sa->sa_family, &family);
                socketAddress.Family = (uint8_t) (family & 0xff);
                socketAddress.Length = len;
                if (len > sizeof(socketAddress.Data))
                {
                    memcpy(socketAddress.Data, sa->sa_data, sizeof(socketAddress.Data));
                }
                else
                {
                    memcpy(socketAddress.Data, sa->sa_data, len);
                }
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
#if defined(WIFI_DEBUG_RECVFROM_DATA) || defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Received %d bytes.", response.Result);
    ESP_LOG_BUFFER_HEX(__func__, response.Buffer, response.Result);
    ESP_LOGI(__func__, "Result: %d, errno: %d", response.Result, response.ResponseErrno);
#endif

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
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Expose the poll method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
void WiFiRequestHandler::Poll(Message *message)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    Esp32Messaging::PollRequest *request = Encoders::ExtractPollRequest(message->Payload);

    struct pollfd *fds = (struct pollfd *) pvPortMalloc(sizeof(struct pollfd));
    int event;    
    fds->fd = request->SocketHandle;
    event = request->Events;
    Mapping::GetEspFlagValue(Mapping::PollEvents, request->Events, &event);
    fds->events = (uint16_t) (event & 0xffff);
    fds->revents = 0;

#if defined(WIFI_FULL_LOGGING) || defined(WIFI_POLL)
    ESP_LOGI(__func__, "Polling socket: %d", request->SocketHandle);
#endif

    Esp32Messaging::PollResponse response;
    response.ResponseErrno = 0;
    response.Result = poll(fds, 1, request->Timeout);
    Mapping::GetStmValue(Mapping::Errno, errno, &response.ResponseErrno);

    Mapping::GetStmFlagValue(Mapping::PollEvents, fds->revents, &event);
    response.ReturnedEvents = (uint16_t) (event & 0xffff);

    vPortFree(fds);
    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedPollResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodePollResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
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
// #if defined(WIFI_FULL_LOGGING)
//     ESP_LOGI(__func__, "Enter");
// #endif

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
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    _currentRetryCount = 0;
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (bits & WIFI_CONNECTING_BIT)
    {
        _currentRequest->DeletePayload();
        _currentRequest->PayloadLength = 3 * sizeof(ip4_addr_t);
        _currentRequest->Payload = static_cast<uint8_t *>(pvPortMalloc(_currentRequest->PayloadLength));
        memcpy(_currentRequest->Payload, &eventData->event_info.got_ip.ip_info.ip, sizeof(ip4_addr_t));
        memcpy(_currentRequest->Payload + sizeof(ip4_addr_t), &eventData->event_info.got_ip.ip_info.netmask, sizeof(ip4_addr_t));
        memcpy(_currentRequest->Payload + 2 * (sizeof(ip4_addr_t)), &eventData->event_info.got_ip.ip_info.gw, sizeof(ip4_addr_t));
        _currentRequest->MessageType = MessageTypes::Response;
        _currentRequest->StatusCode = StatusCodes::CompletedOk;
        _messageDispatcher->QueueMessageForStm32(_currentRequest);
        _currentRequest = nullptr;
    }
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  Process the WiFi disconnected event.
 *
 *  @param eventData
 *      Data for the disconnected event.
 */
void WiFiRequestHandler::StationDisconnectedEvent(system_event_t *eventData)
{
    if (static_cast<bool>(_config.AutomaticReconnect))
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_BIT | WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_RECONNECTING_BIT);
        if (_currentRetryCount < _config.MaximumRetryCount)
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
 *      Event information for the scan complete event.
 */
void WiFiRequestHandler::ScanCompleteEvent(system_event_t *event)
{
#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Number of access points found: %d", apCount);
#endif

    _currentRequest->DeletePayload();
    _currentRequest->MessageType = MessageTypes::Response;
    if (apCount == 0)
    {
        _currentRequest->Payload =  nullptr;
        _currentRequest->PayloadLength = 0;
        _currentRequest->StatusCode = StatusCodes::CompletedOk;
    }
    else
    {
        Esp32Messaging::AccessPointList *accessPointList = static_cast<Esp32Messaging::AccessPointList *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPointList)));
        accessPointList->NumberOfAccessPoints = apCount;
        //
        //  Now we get the access point information.
        //
        wifi_ap_record_t *accessPoints = static_cast<wifi_ap_record_t *>(pvPortMalloc(apCount * sizeof(wifi_ap_record_t)));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, accessPoints));

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
        _currentRequest->StatusCode = StatusCodes::CompletedOk;
        _currentRequest->PayloadLength = Encoders::EncodedAccessPointListBufferSize(accessPointList);
        _currentRequest->Payload = static_cast<uint8_t *>(pvPortMalloc(_currentRequest->PayloadLength));
        Encoders::EncodeAccessPointList(accessPointList, _currentRequest->Payload);
        vPortFree(accessPointList);
    }
    _messageDispatcher->QueueMessageForStm32(_currentRequest);
    _currentRequest = nullptr;
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_BIT);

#if defined(WIFI_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}
