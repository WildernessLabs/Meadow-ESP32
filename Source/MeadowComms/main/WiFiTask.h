#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

//
//  WiFi Function codes
//
#define WIFI_FUNC_START                     0
#define WIFI_FUNC_CONNECT                   1
#define WIFI_FUNC_DISCONNECT                2
#define WIFI_FUNC_GET_ACCESS_POINTS         3
#define WIFI_FUNC_DEEP_SLEEP                4
#define WIFI_FUNC_HTTP_GET                  5
#define WIFI_FUNC_MAX                       5

//
//  Location of the access point information in the buffer returned following
//  a network scan.
//
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

#define DEFAULT_SCAN_METHOD                 WIFI_FAST_SCAN
#define DEFAULT_SORT_METHOD                 WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_RSSI                        -127
#define DEFAULT_AUTHMODE                    WIFI_AUTH_OPEN

//
//  Default number of retires when an error is detected.
//
#define DEFAULT_MAXIMUM_RETRY_COUNT     3

//
//  Maximum length of the string containing the SSID name.
//
#define MAXIMUM_SSID_NAME_LENGTH        32

//
//  Maximum length of the password for the access point.
//
#define MAXIMUM_PASSWORD_LENGTH         32

//
//  Event group bits.
//
#define WIFI_BUSY_BIT                   (BIT0)      /* Indicate if the WiFi task can process another request. */
#define WIFI_CONNECTING_BIT             (BIT1)      /* Indicate if in the process of connecting to a WiFi access point. */
#define WIFI_CONNECTED_BIT              (BIT2)      /* Indicate if the ESP32 is onnected to a WiFi access point.*/
#define WIFI_RECONNECTING_BIT           (BIT3)      /* Indicate if a reconnection (as opposed to a new connection) is being made. */
#define WIFI_INITIALISED_BIT            (BIT4)      /* Indicate if the WiFi system has been initialised. */

/// <summary>
///     Handle for the WiFi messages to be processed by the WIFi task.
/// </summary>
extern QueueHandle_t xWiFiQueueHandle;

/// <summary>
///     Handle for the WiFi task.
/// </summary>
extern TaskHandle_t xWiFiTaskHandle;

//
//  Function prototypes for the WiFi subsystem.
//
void SetupWiFi();
void WiFiTask(void *);