/*
 *  Bluetooth.cpp
 *
 *  Created on: 24 Jan 2019
 *  Author: mark
 */

#include "BluetoothRequestHandler.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

#include "Encoders.hpp"
#include "Exceptions/MultipleInstancesException.hpp"

// static callback methods for now - we'll look to move them into the class once things are working
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

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
bool BluetoothRequestHandler::_instantiated = false;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the BluetoothRequestHandler class.
 */
BluetoothRequestHandler::BluetoothRequestHandler()
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
 *  @brief Create a new BluetoothRequestHandler object.
 *
 *  @param messageDispatcher
 *      Message Dispatcher that can be used to send messages to the STM32.
 */
BluetoothRequestHandler::BluetoothRequestHandler(MessageDispatcher *messageDispatcher) : BluetoothRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/**
 *  @brief Default destructor for the bluetooth class.
 */
BluetoothRequestHandler::~BluetoothRequestHandler()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                           Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Setup the bluetooth system.
 */
void BluetoothRequestHandler::Setup()
{
#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    RequestHandlerBase::Setup();

    _xBtEventGroup = xEventGroupCreate();
    xEventGroupSetBits(_xBtEventGroup, BT_READY_BIT);


    ESP_LOGI(__func__, "mem release BT controller...");

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_LOGI(__func__, "enable BT controller...");
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGI(__func__, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    xTaskCreate(Task, _taskName, 1024 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);

#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Dispatch bluetooth messages to the appropriate method.
 *
 *  @param request
 *      Bluetooth request being made by the STM32.
 */
void BluetoothRequestHandler::DispatchRequest(Message *request)
{
#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    xEventGroupClearBits(_xBtEventGroup, BT_READY_BIT);
    switch (request->Function)
    {
        case BluetoothFunction::Start:
            StartStack(request);
            break;
        case BluetoothFunction::Stop:
            TeardownStack(request);
            break;
        //
        //  Sample from the WiFi class.
        //
        // case WiFiFunction::StartNetwork:
        //     StartNetwork(request);
        //     break;
        default:
            ESP_LOGI(__func__, "Unknown Bluetooth message received, function %d", request->Function);
            if (request->Payload != NULL)
            {
                ESP_LOG_BUFFER_HEXDUMP(__func__, request->Payload, request->PayloadLength, ESP_LOG_INFO);
            }
            break;
    }
    xEventGroupWaitBits(_xBtEventGroup, BT_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Event loop for the IDF system.
 * 
 *  Perform the actual event loop handling on behalf of the static method set up for the
 *  IDF event handler.
 * 
 *  See the notes for the WiFiRequestHandler::EventHandler for more information as to why this
 *  method is necessary.
 * 
 *  @param eventBase
 *      Subsystem that created the event.  This will be one of WIFI_EVENT or IP_EVENT.
 * 
 *  @param eventId
 *      Type of event that has occurred.  This will vary depending upon the subsystem
 *      that generated the event.
 * 
 *  @param eventData
 *      Data specific to the event that has occurred.
 */
void BluetoothRequestHandler::EventHandlerHelper(esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter with base %s and id 0x%x", eventBase, eventId);
#endif

    //     {
    //         case WIFI_EVENT_WIFI_READY:
    //             ESP_LOGI(__func__, "WiFi Ready");
    //             break;

    // if (eventBase == WIFI_EVENT)
    // {
    //     switch (eventId)
    //     {
    //         case WIFI_EVENT_WIFI_READY:
    //             ESP_LOGI(__func__, "WiFi Ready");
    //             break;
    //         case WIFI_EVENT_SCAN_DONE:
    //             ESP_LOGI(__func__, "Scan for access points complete");
    //             xEventGroupSetBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT);
    //             break;
    //         case WIFI_EVENT_STA_START:
    //             ESP_LOGI(__func__, "Station started");
    //             xEventGroupSetBits(_xWiFiEventGroup, WIFI_STATION_STARTED_BIT);
    //             break;
    //         case WIFI_EVENT_STA_STOP:
    //             ESP_LOGI(__func__, "Station stopped");
    //             break;
    //         case WIFI_EVENT_STA_CONNECTED:
    //             ESP_LOGI(__func__, "Station connected to access point");
    //             break;
    //         case WIFI_EVENT_STA_DISCONNECTED:
    //             StationDisconnectedEvent(nullptr);
    //             break;
    //     }
    // }

#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

/**
 *  @brief Event loop for the IDF system.
 * 
 *  Event loop (handler) for the Bluetooth events as discussed in the following ESP-IDF document:
 * 
 *  https://docs.espressif.com/projects/esp-idf/en/release-v4.1/api-reference/system/esp_event.html
 * 
 *  This method passes the data on to an instance helper method.  This is done for convenience as it
 *  allows access to the member variables and methods without having to use the sender context or
 *  convert methods / variables / constants to static members.
 * 
 *  @param arg
 *      Argument data specified when the event loop was first registered with
 *      esp_event_handler_register.  arg should point to the instance of the WiFiRequestHandler
 *      that registered the event loop.
 * 
 *  @param eventBase
 *      Subsystem that created the event.  This will be one of WIFI_EVENT or IP_EVENT.
 * 
 *  @param eventId
 *      Type of event that has occurred.  This will vary depending upon the subsystem
 *      that generated the event.
 * 
 *  @param eventData
 *      Data specific to the event that has occurred.
 */
void BluetoothRequestHandler::EventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    BluetoothRequestHandler *sender = static_cast<BluetoothRequestHandler *>(arg);
    sender->EventHandlerHelper(eventBase, eventId, eventData);

#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

void BluetoothRequestHandler::StartStack(Message *msg)
{
#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    esp_err_t ret;

    ESP_LOGI(__func__, "Enabling BT controller...");

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(__func__, "esp_bt_controller_enable failed %i\n", ret);
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(__func__, "esp_bluedroid_init failed %i\n", ret);
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(__func__, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    // register for GATTS events
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(__func__, "gatts register error, error code = %x", ret);
        return;
    }

    // register for GAP events
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(__func__, "gap register error, error code = %x", ret);
        return;
    }

    // register our one (and only) profile
    ret = esp_ble_gatts_app_register(MEADOW_PROFILE_APP_ID);
    if (ret){
        ESP_LOGE(__func__, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(BT_GATT_MTU);
    if (local_mtu_ret){
        ESP_LOGE(__func__, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif
}

void BluetoothRequestHandler::TeardownStack(Message *msg)
{
#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Enter");
#endif

    esp_err_t ret;

    ret = esp_bt_controller_disable();
    if (ret) {
        ESP_LOGE(__func__, "esp_bt_controller_disable failed %i\n", ret);
        return;
    }

#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif

}

//////////////////////////
//////////////////////////
//////////////////////////

// unknown if there is meaning in this value - pulled from espressif code
#define GATTS_SERVICE_UUID_MEADOW   0x00FF
//#define GATTS_CHAR_UUID_TEST_A      0xFF01
//#define GATTS_DESCR_UUID_TEST_A     0x3333
#define GATTS_NUM_HANDLE_MEADOW     4

// define a struct to hold profile info
// really we'll only use one profile for meadow, but it's a convenient container
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

// NOTE: unknown if this specific value has meaning.  Pulled directly from espressif.
static uint8_t adv_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

// The length of adv data must be less than 31 bytes
// this is what we send out for advertising
static esp_ble_adv_data_t advertising_data = 
{
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// this is what we respond with when scanned - mostly the same as above but no power or intervals
static esp_ble_adv_data_t scan_response_data =
{
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    //.min_interval = 0x0006,
    //.max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static struct gatts_profile_inst meadow_profile =
{
    .gatts_cb = gatts_event_handler,
    .gatts_if = ESP_GATT_IF_NONE /* no interface assigned yet, though it will be 0 when we initialize at ESP_GATTS_REG_EVT */    
};

static esp_ble_adv_params_t advertising_params = {
    .adv_int_min        = 0x20, // this is the minimum advertising interval.  We might need to slow this down for wifi
    .adv_int_max        = 0x40, // this is the maximum advertising interval.  We might need to slow this down for wifi
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static bool advertising_data_configured = false;
static bool response_data_configured = false;

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGI(__func__, "GAP event %i\n", event);
    switch (event)
    {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            // triggered by the gatts_event_handler for ESP_GATTS_REG_EVT where we set the advertising data
            if (advertising_data_configured)
            {
                esp_ble_gap_start_advertising(&advertising_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            // triggered by the gatts_event_handler for ESP_GATTS_REG_EVT where we set the scan data
            if (response_data_configured == 0)
            {
                esp_ble_gap_start_advertising(&advertising_params);
            }
            break; 
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            // advertising has started (well, unless an error is set, so check for that)
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) 
            {
                ESP_LOGE(__func__, "Advertising start failed\n");
            }
            break;
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(__func__, "GATTS event %i\n", event);
    switch(event)
    {
        case ESP_GATTS_REG_EVT:
            {
                // first event to come in.  Configure our advertising params.
                meadow_profile.service_id.is_primary = true;
                meadow_profile.service_id.id.inst_id = 0x00;
                meadow_profile.service_id.id.uuid.len = ESP_UUID_LEN_16;
                meadow_profile.service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_MEADOW;

                // TODO: we likely should add a unique id here
                esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name("CHRIS' MEADOW");
                if (set_dev_name_ret)
                {
                    ESP_LOGE(__func__, "set device name failed, error code = %x", set_dev_name_ret);
                }

                //configure our advertising data
                esp_err_t ret = esp_ble_gap_config_adv_data(&advertising_data);
                if (ret)
                {
                    ESP_LOGE(__func__, "config adv data failed, error code = %x", ret);
                }
                advertising_data_configured = true;

                //configure scan response data - it's the same structure as advertising, but can have different params
                ret = esp_ble_gap_config_adv_data(&scan_response_data);
                if (ret)
                {
                    ESP_LOGE(__func__, "config scan response data failed, error code = %x", ret);
                }
                response_data_configured = true;

                // create our service
                esp_ble_gatts_create_service(gatts_if, &meadow_profile.service_id, GATTS_NUM_HANDLE_MEADOW);
            }
            break;
            /*
        case ESP_GATTS_READ_EVT:
            break;            
        case ESP_GATTS_WRITE_EVT:
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            break;
        case ESP_GATTS_MTU_EVT:
            break;
        case ESP_GATTS_UNREG_EVT:
            break;
        case ESP_GATTS_CREATE_EVT:
            break;
        case ESP_GATTS_ADD_INCL_SRVC_EVT:
            break;
        case ESP_GATTS_ADD_CHAR_EVT:
            break;
        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            break;
        case ESP_GATTS_DELETE_EVT:
            break;
        case ESP_GATTS_START_EVT:
            break;
        case ESP_GATTS_STOP_EVT:
            break;
        case ESP_GATTS_CONNECT_EVT:
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            break;
        case ESP_GATTS_CONF_EVT:
            break;
        case ESP_GATTS_OPEN_EVT:
            break;
        case ESP_GATTS_CANCEL_OPEN_EVT:
            break;
        case ESP_GATTS_CLOSE_EVT:
            break;
        case ESP_GATTS_LISTEN_EVT:
            break;
        case ESP_GATTS_CONGEST_EVT:
            break;
            */
        default:
            break;    
    }    
}
