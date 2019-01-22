/*
 *      WiFi.hpp
 *
 *      Encapsulate the properties and methods required to provide the
 *      ability to communicate with the WiFi network.
 */

#ifndef MAIN_WIFI_HPP_
#define MAIN_WIFI_HPP_

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

#include "Encoders.hpp"
#include "Messaging.h"
#include "ErrorCodes.h"
#include "UART.hpp"
#include "Tasks.hpp"

/*
 *      WiFi Function codes.
 */
#define WIFI_FUNC_START                     0
#define WIFI_FUNC_CONNECT                   1
#define WIFI_FUNC_DISCONNECT                2
#define WIFI_FUNC_GET_ACCESS_POINTS         3
#define WIFI_FUNC_DEEP_SLEEP                4
#define WIFI_FUNC_HTTP_GET                  5
#define WIFI_FUNC_MAX                       5

/*
 *      Location of the access point information in the buffer returned
 *      following a network scan.
 */
#define BSSID_OFFSET                        (0)
#define SSID_OFFSET                         (BSSID_OFFSET + 6)
#define PRIMARY_CHANNEL_OFFSET              (SSID_OFFSET + 33)
#define SECONDARY_CHANNEL_OFFSET            (PRIMARY_CHANNEL_OFFSET + 1)
#define RSSI_OFFSET                         (SECONDARY_CHANNEL_OFFSET + 1)
#define AUTHENTICATION_MODE_OFFSET          (RSSI_OFFSET + 1)
#define PAIRWISE_CIPHER_OFFSET              (AUTHENTICATION_MODE_OFFSET + 1)
#define GROUP_CIPHER_OFFSET                 (PAIRWISE_CIPHER_OFFSET + 1)
#define ANTENNA_OFFSET                      (GROUP_CIPHER_OFFSET + 1)
#define PROTOCOLS_OFFSET                    (ANTENNA_OFFSET + 1)
#define COUNTRY_CODE_OFFSET                 (PROTOCOLS_OFFSET + 4)
#define START_CHANNEL_OFFSET                (COUNTRY_CODE_OFFSET + 3)
#define NUMBER_OF_CHANNELS_OFFSET           (START_CHANNEL_OFFSET + 1)
#define TX_POWER_MAXIMUM_OFFSET             (NUMBER_OF_CHANNELS_OFFSET + 1)
#define POLICY_OFFSET                       (TX_POWER_MAXIMUM_OFFSET + 1)
#define ACCESS_POINT_INFORMATION_LENGTH     (POLICY_OFFSET + 1)

/*
 *      Event group bits.
 */
#define WIFI_READY_BIT                  (BIT0)      /* Indicate if the WiFi task can process another request. */
#define WIFI_CONNECTING_BIT             (BIT1)      /* Indicate if in the process of connecting to a WiFi access point. */
#define WIFI_CONNECTED_BIT              (BIT2)      /* Indicate if the ESP32 is connected to a WiFi access point.*/
#define WIFI_RECONNECTING_BIT           (BIT3)      /* Indicate if a reconnection (as opposed to a new connection) is being made. */
#define WIFI_INITIALISED_BIT            (BIT4)      /* Indicate if the WiFi system has been initialised. */

/*
 *      WiFi class
 *
 *      Encapsulate the properties and methods required to provide the
 *      ability to communicate with the WiFi network.
 */
class WiFi
{
private:
    /*
     *      Default scan method for WiFi networks.
     */
    static const wifi_scan_method_t DefaultScanMethod = WIFI_FAST_SCAN;

    /*
     *      Deafult sort method for the WiFi networks when scanned.
     */
    static const wifi_sort_method_t DefaultSortMethod =
            WIFI_CONNECT_AP_BY_SIGNAL;

    /*
     *      Default RSSI when scanning for networks.
     */
    static const int DefaultRSSI = -127;

    /*
     *      Default authorisation mode.
     */
    static const wifi_auth_mode_t DefaultAuthorisationMode = WIFI_AUTH_OPEN;

    /*
     *  Default number of retires when an error is detected.
     */
    static const int DefaultMaximumRetryCount = 3;


    /*
     *      Maximum length of the string containing the SSID name.
     */
    static const int MaximumSSIDLength = 32;

    /*
     *      Maximum length of the password for the access point.
     */
    static const int MaximumPasswordLength = 64;

    /*
     *      Handle for the WiFi messages to be processed by the WIFi task.
     */
    QueueHandle_t QueueHandle = NULL;

    /*
     *      Event group containing 24-bits used to indicate the state of the WiFi connection.
     */
    EventGroupHandle_t xWiFiEventGroup = NULL;

    /*
     *      Current retry count.
     */
    int _currentRetryCount = 0;

    /*
     *      Maximum number of retries for a failed operation.
     *
     *      This can be overridden using system configuration.
     */
    int _maximumRetryCount = DefaultMaximumRetryCount;

    /*
     *      Indicate if the system should automatically reconnect to the WiFi if we
     *      disconnect for any reason.
     */
    bool _automaticReconnect = true;

    /*
     *      Response message for the current request.
     */
    ApplicationResponse *_wifiResponse = NULL;

    /*
     *      Public methods.
     */
    void SendResponse(uint32_t);
    void ProcessAccessPointScanCompletion(system_event_t *);

    /*
     *      Getters and setters.
     */
    void SetQueueHandle(QueueHandle_t);


public:
    /*
     *      Constructor(s) and destructor.
     */
    WiFi();
    ~WiFi();
    /*
     *      Public methods.
     */
    void Setup();
    void DispatchRequest(struct ApplicationRequest *);
    esp_err_t EventHandler(void *, system_event_t *);
    void StartNetwork(struct ApplicationRequest *);
    void GetAvailableNetworks(struct ApplicationRequest *);
    void HttpGet(struct ApplicationRequest *);

    /*
     *      Getters and setters.
     */
    QueueHandle_t GetQueueHandle() const;
};

/*
 *      External declaration for the global WiFi object.
 */
extern WiFi *wifi;


#endif /* MAIN_WIFI_HPP_ */
