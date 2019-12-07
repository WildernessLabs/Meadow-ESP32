/*
 *  WiFi.hpp
 *
 *  Encapsulate the properties and methods required to provide the
 *  ability to communicate with the WiFi network.
 */

#ifndef _WIFI_REQUEST_HANDLER_HPP_
#define _WIFI_REQUEST_HANDLER_HPP_

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
#include <stdlib.h>

#include "MessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "RequestHandlerBase.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"

/*
 *  @brief WiFi class
 *
 *  Encapsulate the properties and methods required to provide the
 *  ability to communicate with the WiFi network.
 */
class WiFiRequestHandler : public RequestHandlerBase
{
private:
    /**
     *  Name of the FreeRTOS task for the WiFi system.
     */
    const char *_taskName = "WiFiTask";

    /*
     *  Event group bits.
     *
     *  The WIFI_READY_BIT indicates if this current request has completed successfully.  It
     *  is used to block the Task from making a second request when one is already
     *  running.
     *
     *  This flag is necessary as some tasks are actually multi-stage processes.  For
     *  instance, Connecting to a network requires a start request which will generate
     *  two events, network start and got IP.  The request only ends when the IP address
     *  has been granted.
     */
    const int WIFI_READY_BIT = BIT0;
    /**
     *  Indicate if the system is in the process of connecting to an access point.
     */
    const int WIFI_CONNECTING_BIT = BIT1;
    /**
     *  When set, this bit indicates that we have an active WiFi connection.
     */
    const int WIFI_CONNECTED_BIT = BIT2;
    /**
     *  When set, this indicates that the system has lost the WiFi connection
     *  and is actively trying to re-establish the connection.
     */
    const int WIFI_RECONNECTING_BIT = BIT3;
    /**
     *  Indicate if the WiFi system has been initialised OK.
     */
    const int WIFI_INITIALISED_BIT = BIT4;

    /**
     *  Default scan method for WiFi networks.
     */
    const wifi_scan_method_t DEFAULT_SCAN_METHOD = WIFI_FAST_SCAN;

    /**
     *  Default sort method for the WiFi networks when scanned.
     */
    const wifi_sort_method_t DEFAULT_SORT_METHOD = WIFI_CONNECT_AP_BY_SIGNAL;

    /**
     *  Default RSSI when scanning for networks.
     */
    const int DEFAULT_RSSI = -127;

    /**
     *  Default authorisation mode.
     */
    const wifi_auth_mode_t DEFAULT_AUTH_MODE = WIFI_AUTH_OPEN;

    /**
     *  Maximum length of the string containing the SSID name.
     */
    const int MAXIMUM_SSID_LENGTH = 32;

    /**
     *  Maximum length of the password for the access point.
     */
    const int MAXIMUM_PASSWORD_LENGTH = 64;

    /**
     *  GPIO Pin connected to the antenna selection switch
     *  which will connect the on board antenna to the ESP32.
     */
    const gpio_num_t ON_BOARD_ANTENNA_PIN = GPIO_NUM_26;

    /**
     *  GPIO Pin connected to the antenna selection switch
     *  which will connect the external antenna to the ESP32.
     */
    const gpio_num_t EXTERNAL_ANTENNA_PIN = GPIO_NUM_27;

    /**
     *  Current WiFi configuration.
     */
    Esp32Messaging::WiFiConfiguration _config;

    /**
     *  Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /**
     *  Event group containing 24-bits used to indicate the state of the WiFi connection.
     */
    EventGroupHandle_t _xWiFiEventGroup = nullptr;

    /**
     *  Configuration of the WiFi network.
     */
    wifi_config_t *_wifiConfig = nullptr;

    /**
     *  Request currently being processed by the WiFi system.
     */
    Message *_currentRequest = nullptr;

    /**
     *  Current retry count.
     */
    int _currentRetryCount = 0;

    /**
     *  Read the configuration from non-volatile storage.
     */
    void ReadConfigFromNonVolatileStorage();

    /*
     *  Private methods.
     */
    void SendResponse(Message *, uint32_t);

    /*
     *  Convert the Antenna enum representation of an antenna
     *  into a GPIO pin number.
     */
    gpio_num_t ConvertAntennaToPin(AntennaTypes::AntennaTypes);

    /*
     *  This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *) override;

    /*
     *  Start the network.
     */
    void StartNetwork(Message *);

    /*
     *  Get a list of available access points.
     */
    void GetAvailableNetworks(Message *);

    /*
     *  Getters and setters for the antenna in use.
     */
    void GetAntenna(Message *) const;
    void SetAntenna(Message *);

    /**
     *  Chnage the current antenna and activate it.
     */
    void SwitchAntenna(AntennaTypes::AntennaTypes);

    /*
     *  Private constructor for the class.
     */
    WiFiRequestHandler();

    /*
     *  Private destructor for the class.
     */
    ~WiFiRequestHandler();

    /*
     *  Process the WiFi access point scan complete event.
     */
    void ScanCompleteEvent(system_event_t *);

    /*
     *  Process the WiFi connecting and getting an IP address event
     */
    void StationGotIpEvent(system_event_t *);

    /*
     *  Processes the station disconnected event.
     */
    void StationDisconnectedEvent(system_event_t *);

    //----------------------------------------------------------------------
    //
    //                      POSIX Calls start here.
    /**
     *  Provide a mechanism for the STM32 to call the getaddrinfo method.
     */
    void GetAddrInfo(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the socket method.
     */
    void Socket(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the connect method.
     */
    void Connect(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the freeaddrinfo method.
     */
    void FreeAddrInfo(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the write method.
     */
    void Write(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the setsockopt method.
     */
    void SetSockOpt(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the read method.
     */
    void Read(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the close method.
     */
    void Close(Message *);

public:
    /*
     *  Constructor for the WiFi object.
     */
    explicit WiFiRequestHandler(MessageDispatcher *);

    /*
     *  Public methods.
     */
    void Setup();

    /*
     *  This is the event handler called by FreeRTOS.  It merely
     *  passes the event on to an internal private event handler
     *  within the class.
     */
    esp_err_t EventHandler(void *, system_event_t *);
};

#endif /* _WIFI_REQUEST_HANDLER_HPP_ */
