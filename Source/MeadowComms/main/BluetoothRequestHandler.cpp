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
//static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
//static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

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
BluetoothConfig BluetoothRequestHandler::Config;

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

    // copy over all parameters
    Esp32Messaging::BTServicesDescription *description = Encoders::ExtractBTServicesDescription(msg->Payload);
    Config.SetDeviceName(description->DeviceName);

    esp_err_t ret;

    ESP_LOGI(COMPONENT_NAME, "Enabling BT controller for '%s'", description->DeviceName);

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(COMPONENT_NAME, "esp_bt_controller_enable failed %i\n", ret);
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(COMPONENT_NAME, "esp_bluedroid_init failed %i\n", ret);
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(COMPONENT_NAME, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    // register for GATTS events
    ret = esp_ble_gatts_register_callback(this->gatts_profile_event_handler);
    if (ret){
        ESP_LOGE(COMPONENT_NAME, "gatts register error, error code = %x", ret);
        return;
    }

    // register for GAP events
    ret = esp_ble_gap_register_callback(this->gap_event_handler);
    if (ret){
        ESP_LOGE(COMPONENT_NAME, "gap register error, error code = %x", ret);
        return;
    }

    // register our one (and only) profile
    ret = esp_ble_gatts_app_register(MEADOW_PROFILE_APP_ID);
    if (ret){
        ESP_LOGE(COMPONENT_NAME, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(BT_GATT_MTU);
    if (local_mtu_ret){
        ESP_LOGE(COMPONENT_NAME, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    //set static passkey
    uint32_t passkey = 123456;
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));

    /* If your BLE device acts as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the master;
    If your BLE device acts as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

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
        ESP_LOGE(COMPONENT_NAME, "esp_bt_controller_disable failed %i\n", ret);
        return;
    }

#if defined(BT_FULL_LOGGING)
    ESP_LOGI(__func__, "Exit");
#endif

}

void BluetoothRequestHandler::AddCharacteristic(Message *msg)
{
    // copy over all parameters
    Esp32Messaging::BTServicesDescription *description = Encoders::ExtractBTServicesDescription(msg->Payload);
    Config.SetDeviceName(description->DeviceName);
}

//////////////////////////
//////////////////////////
//////////////////////////


// we need a profile array

// each profile contains 1 service

// each service contains N chacratcteristics



// unknown if there is meaning in this value - pulled from espressif code
#define GATTS_SERVICE_UUID_MEADOW   0x00FF
#define GATTS_CHARACTERISTIC_UUID_MEADOW      0xFF01
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
static uint8_t adv_service_uuid[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
};

static uint8_t meadow_manufacturer[10]={'W', 'I', 'L', 'D', 'E', 'R', 'N', 'E', 'S', 'S'};

// The length of adv data must be less than 31 bytes
// this is what we send out for advertising
static esp_ble_adv_data_t advertising_data = 
{
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid),
    .p_service_uuid = adv_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// this is what we respond with when scanned - mostly the same as above but no power or intervals
static esp_ble_adv_data_t scan_response_data =
{
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .manufacturer_len = sizeof(meadow_manufacturer),
    .p_manufacturer_data = meadow_manufacturer,
};

static esp_ble_adv_params_t advertising_params = {
    .adv_int_min        = 0x20, // this is the minimum advertising interval.  We might need to slow this down for wifi
    .adv_int_max        = 0x40, // this is the maximum advertising interval.  We might need to slow this down for wifi
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_RANDOM, // BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static bool advertising_data_configured = false;
static bool response_data_configured = false;

// --- test property stuff. We'll modify this when managed starts integrating
#define GATTS_CHAR_VAL_LEN_MAX 0x40 // not sure if this is a BT thing, or just arbitrary. Assuming arbitrary for now

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t characteristic_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE|ESP_GATT_CHAR_PROP_BIT_READ;

/// Sensor Service - Heart Rate Measurement Characteristic, notify
static const uint16_t heart_rate_meas_uuid = ESP_GATT_HEART_RATE_MEAS;
static const uint8_t heart_measurement_ccc[2] ={ 0x00, 0x00};


/// Sensor Service - Sensor Location characteristic, read
static const uint16_t sensor_location_uuid = ESP_GATT_UUID_SENSOR_LOCATION;
static const uint8_t sensor_loc_val[1] = {0x00};


/// Heart Rate Sensor Service - Heart Rate Control Point characteristic, write&read
static const uint16_t heart_rate_ctrl_point = ESP_GATT_HEART_RATE_CNTL_POINT;
static const uint8_t heart_ctrl_point[1] = {0x00};

#define HRPS_HT_MEAS_MAX_LEN            (13)

static const esp_gatts_attr_db_t meadow_attribute_list[8] =
{
    // Service Declaration - all services present the UUID
    [0]                    =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(adv_service_uuid), (uint8_t *)&adv_service_uuid}},

    // Characteristic Declaration
    [1]            =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify}},

    // Characteristic Value
    [2]             =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&heart_rate_meas_uuid, ESP_GATT_PERM_READ,
      HRPS_HT_MEAS_MAX_LEN, 0, NULL}},

    // Heart Rate Measurement Characteristic - Client Characteristic Configuration Descriptor
    [3]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
      sizeof(uint16_t),sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},

    // Body Sensor Location Characteristic Declaration
    [4]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},

    // Sensor Location Characteristic Value
    [5]   =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&sensor_location_uuid, ESP_GATT_PERM_READ_ENCRYPTED,
      sizeof(uint8_t), sizeof(sensor_loc_val), (uint8_t *)sensor_loc_val}},

    // Heart Rate Control Point Characteristic Declaration
    [6]          =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

    // Heart Rate Control Point Characteristic Value
    [7]             =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&heart_rate_ctrl_point, ESP_GATT_PERM_WRITE_ENCRYPTED|ESP_GATT_PERM_READ_ENCRYPTED,
      sizeof(uint8_t), sizeof(heart_ctrl_point), (uint8_t *)heart_ctrl_point}},
};

void BluetoothRequestHandler::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGI(COMPONENT_NAME, "GAP event %i", event);
    switch (event)
    {
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            if (advertising_data_configured)
            {
                esp_ble_gap_start_advertising(&advertising_params);
            }
        break;
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: // 0
            // triggered by the gatts_event_handler for ESP_GATTS_REG_EVT where we set the advertising data
            if (advertising_data_configured)
            {
                esp_ble_gap_start_advertising(&advertising_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: // 6
            // advertising has started (well, unless an error is set, so check for that)
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) 
            {
                ESP_LOGE(COMPONENT_NAME, "Advertising start failed\n");
            }
            ESP_LOGI(COMPONENT_NAME, "Advertising start success\n");
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            /* passkey request event */
            ESP_LOGI(COMPONENT_NAME, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
            /* Call the following function to input the passkey which is displayed on the remote device */
            //esp_ble_passkey_reply(heart_rate_profile_tab[HEART_PROFILE_APP_IDX].remote_bda, true, 0x00);
            break;
        case ESP_GAP_BLE_OOB_REQ_EVT:
        {
            ESP_LOGI(COMPONENT_NAME, "ESP_GAP_BLE_OOB_REQ_EVT");
            uint8_t tk[16] = {1}; //If you paired with OOB, both devices need to use the same tk
            esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
        }
        break;
        case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
            ESP_LOGI(COMPONENT_NAME, "ESP_GAP_BLE_LOCAL_IR_EVT");
            break;
        case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
            ESP_LOGI(COMPONENT_NAME, "ESP_GAP_BLE_LOCAL_ER_EVT");
            break;
        case ESP_GAP_BLE_NC_REQ_EVT: // 16
            /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
            show the passkey number to the user to confirm it with the number displayed by peer device. */
            esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
            ESP_LOGI(COMPONENT_NAME, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT: // 10
            // security request 
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: // 11
            ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
            ///show the passkey number to the user to input it in the peer device.
            ESP_LOGI(COMPONENT_NAME, "The passkey Notify number:%d", param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_KEY_EVT: // 9
            //shows the ble key info share with peer device to the user.
            ESP_LOGI(COMPONENT_NAME, "key type = %d", param->ble_security.ble_key.key_type);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT: // 8
        {
            // auth complete
            if(param->ble_security.auth_cmpl.success) 
            {
                ESP_LOGI(COMPONENT_NAME, "Auth success. mode = %d", param->ble_security.auth_cmpl.auth_mode);
            }
            else
            {
                ESP_LOGE(COMPONENT_NAME, "Auth failed. reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
            }
        }
        break;
        case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT: 
        {
            ESP_LOGD(COMPONENT_NAME, "ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT status = %d", param->remove_bond_dev_cmpl.status);
            ESP_LOGI(COMPONENT_NAME, "ESP_GAP_BLE_REMOVE_BOND_DEV");
            ESP_LOGI(COMPONENT_NAME, "-----ESP_GAP_BLE_REMOVE_BOND_DEV----");
            esp_log_buffer_hex(COMPONENT_NAME, (void *)param->remove_bond_dev_cmpl.bd_addr, sizeof(esp_bd_addr_t));
            ESP_LOGI(COMPONENT_NAME, "------------------------------------");
            break;
        }
        case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT: // 22
        {
            if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS)
            {
                ESP_LOGE(COMPONENT_NAME, "config local privacy failed, error status = %x", param->local_privacy_cmpl.status);
                break;
            }

            esp_err_t ret = esp_ble_gap_config_adv_data(&advertising_data);
            if (ret)
            {
                ESP_LOGE(COMPONENT_NAME, "config adv data failed, error code = %x", ret);
            }
            else
            {
                advertising_data_configured = true;
            }

            ret = esp_ble_gap_config_adv_data(&scan_response_data);
            if (ret)
            {
                ESP_LOGE(COMPONENT_NAME, "config adv data failed, error code = %x", ret);
            }
            else
            {
                response_data_configured = true;
            }
        }
        break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: // 20
            ESP_LOGI(COMPONENT_NAME, "GAP connection parameters successfully changed\n");
            break;
        default:
            break;
    }
}

static uint16_t attribute_handle_table[8];

void BluetoothRequestHandler::gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if, 
                                        esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGD(__func__, "event = %x\n", event);
    switch (event) 
    {
        case ESP_GATTS_REG_EVT:
        {
            esp_ble_gap_set_device_name(BluetoothRequestHandler::Config.GetDeviceName());
            //generate a resolvable random address
            esp_ble_gap_config_local_privacy(true);
            esp_ble_gatts_create_attr_tab(meadow_attribute_list, gatts_if, 8, 0);
        }
            break;
        case ESP_GATTS_READ_EVT:
            break;
        case ESP_GATTS_WRITE_EVT:
        {
            ESP_LOGI(COMPONENT_NAME, "ESP_GATTS_WRITE_EVT, write value:");
            esp_log_buffer_hex(__func__, param->write.value, param->write.len);
        }
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            break;
        case ESP_GATTS_MTU_EVT:
            break;
        case ESP_GATTS_CONF_EVT:
            break;
        case ESP_GATTS_UNREG_EVT:
            break;
        case ESP_GATTS_DELETE_EVT:
            break;
        case ESP_GATTS_START_EVT:
            break;
        case ESP_GATTS_STOP_EVT:
            break;
        case ESP_GATTS_CONNECT_EVT:
        {
            ESP_LOGI(COMPONENT_NAME, "ESP_GATTS_CONNECT_EVT");
            /* start security connect with peer device when receive the connect event sent by the master */
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
        }
            break;
        case ESP_GATTS_DISCONNECT_EVT:
        {
            ESP_LOGI(COMPONENT_NAME, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
            /* start advertising again when missing the connect */
            esp_ble_gap_start_advertising(&advertising_params);
        }
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
        case ESP_GATTS_CREAT_ATTR_TAB_EVT: 
        {
            ESP_LOGI(COMPONENT_NAME, "The number handle = %x",param->add_attr_tab.num_handle);
            if (param->create.status == ESP_GATT_OK)
            {
                if(param->add_attr_tab.num_handle == 8) 
                {
                    memcpy(attribute_handle_table, param->add_attr_tab.handles,
                    sizeof(attribute_handle_table));
                    esp_ble_gatts_start_service(attribute_handle_table[0]);
                }
                else
                {
                    ESP_LOGE(COMPONENT_NAME, "Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)",
                         param->add_attr_tab.num_handle, 8);
                }
            }
            else
            {
                ESP_LOGE(COMPONENT_NAME, " Create attribute table failed, error code = %x", param->create.status);
            }
        }
        break;
        default:
        break;
    }
}