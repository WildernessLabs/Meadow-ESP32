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
#include "esp_event.h"
#include "errno.h"
#include <string.h>
#include <stdlib.h>

#include "MessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "RequestHandlerBase.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"
#include "GenericList.hpp"

/**
 *  The following definitions are copied from the NuttX system as they
 *  have no analogy in ESP-IDF and hence no mapping is needed but we need
 *  a copy of these defintions in order to implement some of the networking
 *  functionality.
 */

/* Interface flag bits (for ioctl) */

#define IFF_DOWN           (1 << 0) /* Interface is down */
#define IFF_UP             (1 << 1) /* Interface is up */
#define IFF_RUNNING        (1 << 2) /* Carrier is available */
#define IFF_IPv6           (1 << 3) /* Configured for IPv6 packet (vs ARP or IPv4) */
#define IFF_NOARP          (1 << 7) /* ARP is not required for this packet */

/* Socket definitions (for ioctl) */

#define _SIOCBASE           (0x0700)            /* Socket ioctl commands */
#define _IOC(type,nr)       ((type)|(nr))
#define _SIOC(nr)           _IOC(_SIOCBASE,nr)

#define SIOCGIFCONF         _SIOC(0x0018)       /* Return an interface list (IPv4) */
#define SIOCGIFFLAGS        _SIOC(0x001b)       /* Gets the interface flags */

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
     *  Structure used to hold information about a poll request.
     */
    struct PollRequestInformation
    {
        /**
         *  List of socket handles that were used in this poll request.
         */
        struct pollfd *Fds;

        /**
         *  Events that this poll request is pending.
         */
        uint16_t Events;

        /**
         *  Result from the poll request.
         */
        int PollResult = 0;

        /**
         *  Keep a copy of errno from the request.
         */
        int PollErrno = 0;

        /**
         *  Timeout for the poll request.
         */
        int Timeout;

        /**
         *  Message that raised the original poll request.
         */
        int OriginalRequestID;

        /**
         *  Has a teardown been requested?
         * 
         *  This is used to distinguish between a teardown request from the
         *  STM32 and a normal poll event being received.
         */
        bool Teardown;

        /**
         *  Handle for the task that is dealing with the poll request.
         */
        TaskHandle_t TaskHandle = nullptr;
    };

    /**
     *  Name of the FreeRTOS task for the WiFi system.
     */
    const char *TASK_NAME = "WiFiTask";

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
    const int WIFI_INTERFACE_STARTING_BIT = BIT4;

    /**
     *  Indicate if the WiFi system has been initialised OK.
     */
    const int WIFI_INTERFACE_STARTED_BIT = BIT5;

    /**
     *  Indicate if the scan for access points has completed.
     */
    const int WIFI_SCAN_COMPLETE_BIT = BIT6;

    /**
     *  Indicate that the WiFi station has started successfully.
     */
    const int WIFI_STATION_STARTED_BIT = BIT7;

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
    static const gpio_num_t ON_BOARD_ANTENNA_PIN;

    /**
     *  GPIO Pin connected to the antenna selection switch
     *  which will connect the external antenna to the ESP32.
     */
    static const gpio_num_t EXTERNAL_ANTENNA_PIN;

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
     *  Network Interface Handle for the board when running in station mode
     *  i.e. as a device connected to an access point.
     */
    esp_netif_t *_stationNetIfHandle;

    /**
     *  Network Interface Handle for the board when it is acting as a soft
     *  access point.
     */
    esp_netif_t *_softApNetIfHandle;

    /**
     *  Name of the device on the network.
     */
    static char *_deviceName;

    /**
     *  Name of the storage in NVS holding the name of the device.
     */
    static const char *DEVICE_NAME_NAME;

    /**
     *  Current antenna in use.  Default to the on board antenna at startup.
     */
    static AntennaTypes::AntennaTypes _currentAntenna;

    /**
     *  Name of the NVS storage holding the _currentAntenna value.
     */
    static const char *ANTENNA_TYPE_NAME;

    /**
     *  Should the system automatically reconnect to an access point should there
     *  be a problem and the current connection is dropped.
     */
    static bool _automaticReconnect;

    /**
     *  Name of the NVS storage holding the _automaticReconnect value.
     */
    static const char *AUTOMATIC_RECONNECT_NAME;

    /**
     *  Indicate if the network should start automatically on reboot.
     */
    static bool _automaticallyStartNetwork;

    /**
     *  Name of the NVS storage holding the _automaticallyStartNetwork value.
     */
    static const char *AUTOMATICALLY_START_NETWORK_NAME;

    /**
     *  Maximum number of times a failed event should retry before raising an error.
     */
    static uint32_t _maximumRetryCount;

    /**
     *  Name of the NVS storage holding the _maximumRetryCount value.
     */
    static const char *MAXIMUM_RETRY_COUNT_NAME;

    /**
     *  Indicate if the system should connect to a time server and get the
     *  current time at startup.
     */
    static bool _getTimeAtStartup;

    /**
     *  Name fo the storage in NVS that holds the value indicating that the
     *  system should get the time at startup. 
     */
    static const char *GET_TIME_AT_STARTUP_NAME;

    /**
     *  Indicate if we should use DHCP.
     */
    static bool _useDhcp;

    /**
     *  Name of the storage holding the _useDhcp configuration setting.
     */
    static const char *USE_DHCP_NAME;

    /**
     *  Defaut access [oint to be used when automatically connecting.
     */
    static char *_defaultAccessPoint;

    /**
     *  Name of the storage in NVS holding the default access point.
     */
    static const char *DEFAULT_ACCESS_POINT_NAME;

    /**
     *  Password for the access point.
     */
    static char *_password;

    /**
     *  Name of the storage in NVS holding the password for the access point.
     */
    static const char *PASSWORD_NAME;

    /**
     *  Name of the NTP server to be used when using NTP to set the time
     *  when the board starts.
     */
    static char *_ntpServer;

    /**
     *  Name of the storage space in NVS holding the timezone information.
     */
    static const char *TIMEZONE_NAME;

    /**
     *  Timezone to be used when the ESP32 starts.  Default will be UTC.
     */
    static char *_timezone;

    /**
     *  Name of the storage space in NVS holding the NTP server information.
     */
    static const char *NTP_SERVER_NAME;

    /**
     *  Static IP address when not using DHCP.
     */
    static uint32_t _staticIpAddress;

    /**
     *  Name of the storage space holding the static IP address.
     */
    static const char *STATIC_IP_ADDRESS_NAME;

    /**
     *  DNS server when not using DHCP.
     */
    static uint32_t _dnsServer;

    /**
     *  Name of the storage in NVS holding the DNS server IP address.
     */
    static const char *DNS_SERVER_NAME;

    /**
     *  IP address of the default gateway when not using DHCP.
     */
    static uint32_t _defaultGateway;

    /**
     *  Name of the storage space holding the IP address of the default gateway.
     */
    static const char *DEFAULT_GATEWAY_NAME;

    /**
     *  Default value for the WiFi timeout.
     */
    static uint32_t _wifiTimeout;

    /**
     *  Name of the storage space holding the default WiFi timeout.
     */
    static const char *DEFAULT_WIFI_TIMEOUT_NAME;

    /**
     *  List of active poll requests.
     */
    static GenericLinkList<PollRequestInformation> *_pollRequests;

    /**
     *  Mutex for the poll request linked list.
     */
    static SemaphoreHandle_t _pollRequestListMutex;

    /**
     *  Handle for the completed poll requests.
     */
    static QueueHandle_t _completedPollRequestsQueue;

    /**
     *  Method that will deal with completed poll requests.
     */
    TaskHandle_t _completedPollRequestsTaskHandle;

    /*
     *  Private methods.
     */
    // void SendResponse(Message *, uint32_t);

    /*
     *  This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *) override;

    /*
     *  Instance helper method for the static EventHandler method.
     */
    void EventHandlerHelper(esp_event_base_t, int32_t, void *);

    /**
     *  Method that will compare two poll requests IDs and determine if they are the same.
     * 
     *  This method is used with the list of poll requests.
     */
    static bool ComparePollRequestMessageIds(uint32_t, PollRequestInformation *);

    /**
     *  Disconnect from the current access point.
     */
    void DisconnectFromAccessPoint(Message *);

    /**
     *  Connect to an access point.
     */
    void ConnectToAccessPoint(Message *);

    /**
     *  Stop the WiFi and release all resources.
     */
    void Stop(Message *);

    /*
     *  Start the network.
     */
    void StartNetwork(Message *);

    /*
     *  Get a list of available access points.
     */
    void GetAvailableNetworks(Message *);

    /**
     *  Convert the Antenna enum representation of an antenna
     *  into a GPIO pin number.
     */
    static gpio_num_t ConvertAntennaToPin(AntennaTypes::AntennaTypes);

    /**
     *  Change the current antenna and activate it.
     */
    static void SwitchAntenna(AntennaTypes::AntennaTypes);

    /*
     *  Private constructor for the class.
     */
    WiFiRequestHandler();

    /*
     *  Private destructor for the class.
     */
    ~WiFiRequestHandler();

    /*
     *  Process the WiFi connecting and getting an IP address event
     */
    void StationGotIpEvent();

    /*
     *  Connect to the specified access point starting the network interface if necessary.
     */
    void ConnectToAccessPoint(char *, char *);

    /*
     *  Processes the station disconnected event.
     */
    void StationDisconnectedEvent(system_event_t *);

    /**
     *  Method that will deal with poll requests.
     */
    static void PollRequestProcessor(void *);

    /**
     *  Method that will deal with completed poll requests.
     */
    static void CompletedPollRequestProcessor(void *);

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

    /**
     *  Provide a mechanism for the STM32 to call the sendto method.
     */
    void SendTo(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the send method.
     */
    void Send(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the recvfrom method.
     */
    void RecvFrom(Message *);

    /**
     *  Provide a mechanism for the STM32 to call the poll method.
     */
    void Poll(Message *);

    /**
     * Provide a mechanism for the STM32 to call the bind method.
     */
    void Bind(Message *);

    /**
     * Provide a mechanism for the STM32 to call the listen method.
     */
    void Listen(Message *);

    /**
     * Provide a mechanism for the STM32 to call the accept method.
     */
    void Accept(Message *);

    /**
     * Provide a mechanism for the STM32 to call the simulated ioctl method.
     */
    void Ioctl(Message *);

    /**
     * Provide a mechanism for the STM32 to call the simulated getsockname method.
     */
    void GetSockName(Message *);

public:
    /*
     *  Constructor for the WiFi object.
     */
    explicit WiFiRequestHandler(MessageDispatcher *);

    /*
     *  Public methods.
     */
    void Setup();

    /**
     *  Start the network interface ready for the device to be configured
     *  as either an access point (softAP) or as a network device (station).
     */
    esp_err_t StartWiFiInterface();

    /*
     *  This is the event handler called by FreeRTOS.  It merely
     *  passes the event on to an internal private event handler
     *  within the class.
     */
    static void EventHandler(void *, esp_event_base_t, int32_t, void *);

    /**
     *  Get / Set the maximum number of retry attempts.
     */
    static uint32_t GetMaximumRetryCount();
    static StatusCodes::StatusCodes SetMaximumRetryCount(uint32_t);

    /**
     *  Get / Set the automatic reconnect property.
     */
    static bool GetAutomaticReconnect();
    static StatusCodes::StatusCodes SetAutomaticReconnect(bool);

    /**
     *  Get / Set the automatically start network property.
     */
    static bool GetAutomaticallyStartNetwork();
    static StatusCodes::StatusCodes SetAutomaticallyStartNetwork(bool);

    /**
     *  Get / Set the get time a startup configuration value.
     */
    static bool GetGetTimeAtStartup();
    static StatusCodes::StatusCodes SetGetTimeAtStartup(bool);

    /**
     *  Get / Set the get time a startup configuration value.
     */
    static bool GetUseDhcp();
    static StatusCodes::StatusCodes SetUseDhcp(bool);

    /**
     *  Get / Set the NTP server to be used when getting the time
     *  at startup.
     */
    static char *GetNtpServer();
    static StatusCodes::StatusCodes SetNtpServer(char *);

    /**
     *  Get / Set the timezone for the system.
     */
    static char *GetTimezone();
    static StatusCodes::StatusCodes SetTimezone(char *);

    /**
     *  Get / Set the name of the device on the network.
     */
    static char *GetDeviceName();
    static StatusCodes::StatusCodes SetDeviceName(char *);
    
    /**
     *  Get / Set the name of the default access point to be used when
     *  the board starts.
     */
    static char *GetDefaultAccessPoint();
    static StatusCodes::StatusCodes SetDefaultAccessPoint(char *);

    /**
     *  Get / Set the password for the default access point.
     */
    static char *GetPassword();
    static StatusCodes::StatusCodes SetPassword(char *);

    /**
     *  Get / set the default IP address when not using DHCP.
     */
    static uint32_t GetIpAddress();
    static StatusCodes::StatusCodes SetIpAddress(uint32_t);

    /**
     *  Get / set the IP address of the DNS server to be used when not using DHCP.
     */
    static uint32_t GetDnsServer();
    static StatusCodes::StatusCodes SetDnsServer(uint32_t);

    /**
     *  Get / set the IP address of the default gateway.
     */
    static uint32_t GetDefaultGateway();
    static StatusCodes::StatusCodes SetDefaultGateway(uint32_t);

    /**
     *  Get / Set the MAC address of the interface.
     */
    static void GetMacAddress(uint8_t *, esp_interface_t);
    static StatusCodes::StatusCodes SetMacAddress(uint8_t *, esp_interface_t);

    /**
     *  Get / Set the current antenna in use.
     */
    static uint8_t GetAntenna();
    static StatusCodes::StatusCodes SetAntenna(uint8_t);

    /**
     *  Get / Set the WiFi timeout value.
     */
    static uint32_t GetWiFiTimeout();
    static StatusCodes::StatusCodes SetWiFiTimeout(uint32_t);
};

#endif /* _WIFI_REQUEST_HANDLER_HPP_ */
