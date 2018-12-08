#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include <string.h>

#include "WiFiTask.h"
#include "Messaging.h"
#include "UARTTask.h"
#include "Encoders.h"

/// <summary>
///     Handle for the WiFi messages to be processed by the WIFi task.
/// </summary>
QueueHandle_t WiFiQueueHandle = NULL;

/// <summary>
///     Handle for the WiFi task.
/// </summary>
TaskHandle_t xWiFiTaskHandle = NULL;

#define DEFAULT_MAXIMUM_RETRY       3

#if CONFIG_WIFI_ALL_CHANNEL_SCAN
#define DEFAULT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#elif CONFIG_WIFI_FAST_SCAN
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#else
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#endif /*CONFIG_SCAN_METHOD*/

#if CONFIG_WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_WIFI_CONNECT_AP_BY_SECURITY
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#else
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#endif /*CONFIG_SORT_METHOD*/

#if CONFIG_FAST_SCAN_THRESHOLD
#define DEFAULT_RSSI CONFIG_FAST_SCAN_MINIMUM_SIGNAL
#if CONFIG_EXAMPLE_OPEN
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#elif CONFIG_EXAMPLE_WEP
#define DEFAULT_AUTHMODE WIFI_AUTH_WEP
#elif CONFIG_EXAMPLE_WPA
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA_PSK
#elif CONFIG_EXAMPLE_WPA2
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA2_PSK
#else
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif
#else
#define DEFAULT_RSSI -127
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif /*CONFIG_FAST_SCAN_THRESHOLD*/

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

const char *WIFI_TASK_NAME = "WiFiTask";

static int s_retry_num = 0;

static struct ApplicationResponse *_wifiResponse = NULL;

void ProcessAccessPointScanCompletion(system_event_t *event)
{
    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);
    printf("Number of access points found: %d\n", event->event_info.scan_done.number);
    if (apCount == 0)
    {
        _wifiResponse->payload = NULL;
        _wifiResponse->payloadLength = 0;
    }
    else
    {
        uint16_t apRecordSize = sizeof(wifi_ap_record_t);
        //
        //  First two elements in this payload are the number of access points
        //  and the size of the access point record.
        //
        _wifiResponse->payloadLength = (apRecordSize * apCount) + 4;
        _wifiResponse->payload = pvPortMalloc(_wifiResponse->payloadLength);
        EncodeUInt16(apCount, _wifiResponse->payload);
        EncodeUInt16(apRecordSize, _wifiResponse->payload + 2);
        //
        //  Now we get the access point information.
        //
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, (wifi_ap_record_t *) (_wifiResponse->payload + 4)));
    }
    QueueUARTResponse(_wifiResponse);
    vPortFree(_wifiResponse);
    _wifiResponse = NULL;
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(__func__, "Got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            s_retry_num = 0;
            _wifiResponse->payloadLength = sizeof(ip4_addr_t);
            _wifiResponse->payload = pvPortMalloc(_wifiResponse->payloadLength);
            memcpy(_wifiResponse->payload, &event->event_info.got_ip.ip_info.ip, _wifiResponse->payloadLength);
            QueueUARTResponse(_wifiResponse);
            vPortFree(_wifiResponse);
            _wifiResponse = NULL;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            if (s_retry_num < DEFAULT_MAXIMUM_RETRY)
            {
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                s_retry_num++;
                ESP_LOGI(__func__, "Retrying to connect to the AP");
            }
            ESP_LOGI(__func__, "Connect to the AP fail\n");
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
    char *ssid = (char *) message->payload;
    if (ssid == NULL)
    {
        //  TODO: Deal with error.
    }

    char *password = (char *) (message->payload + strlen(ssid) + 1);
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

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
    //
    //  TODO: Check lengths.
    //
    memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
    memcpy(wifi_config.sta.password, password, strlen(password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/// <summary>
///     Get the full list of available networks.
/// </summary>
/// <remarks>
///     The network must be started before this method is called.
/// </remarks>
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
///     Task to process the WiFi messages.
/// <summary>
void WiFiTask(void *pvParameters)
{
    s_wifi_event_group = xEventGroupCreate();
    while (true)
    {
        struct ApplicationRequest message;
        BaseType_t result = xQueueReceive(WiFiQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            switch (message.function)
            {
                case WIFI_FUNC_START:
                    ESP_LOGI(__func__, "Start WiFi request received.");
                    StartNetwork(&message);
                    break;
                case WIFI_FUNC_GET_ACCESS_POINTS:
                    GetAvailableNetworks(&message);
                    break;
                default:
                    ESP_LOGI(__func__, "WiFi message received, function %d", message.function);
                    //
                    //  Memory for the payload is allocated on the heap and it needs to be released.
                    //
                    if(message.payload != NULL)
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
