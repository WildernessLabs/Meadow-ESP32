/**
 * @file BluetoothRequestHandler.cpp
 * @author Mark Stevens
 * @brief Implementation for the Bluetooth Request Handler class.
 * @date 2025-01-27
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#include "sdkconfig.h"

#include "BluetoothRequestHandler.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdlib.h>

#include "Globals.hpp"
#include "Logging.hpp"
#include "Encoders.hpp"
#include "Mapping.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "BluetoothComponentName.hpp"
#include "Utils.hpp"

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

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
BluetoothManager BluetoothRequestHandler::_manager;

/**
 * @brief Pointer to this instance when instantiated.
 */
BluetoothRequestHandler *BluetoothRequestHandler::_instance = nullptr;

/**
 * @brief Name of the component for logging purposes.
 */
const char *BluetoothRequestHandler::COMPONENT_NAME = BLUETOOTH_COMPONENT_NAME;

/**
 * @brief Set the handle for the GATTS interface to indicate that it is not valid.
 */
esp_gatt_if_t BluetoothRequestHandler::_gattsInterfaceHandle = ESP_GATT_IF_NONE;

/**
 * @brief The handle that is assigned a connection when a device connects over Bluetooth.
 */
uint16_t BluetoothRequestHandler::_connectionHandle = INVALID_CONNECTION_HANDLE;

/**
 * @brief Indicate if we have configured the advertising data.
 */
bool BluetoothRequestHandler::_advertisingDataConfigured = false;

/**
 * @brief Has the response data been configured?
 */
bool BluetoothRequestHandler::_responseDataConfigured = false;

/**
 * @brief Array holding the manufacturer data for these services.  This currently holds the two byte client ID.
 */
uint8_t BluetoothRequestHandler::_manufacturerData[2] = {};

/**
 * @brief Dummy UUID for the advertising service.  This is taken from the ESP-IDF examples.
 */
// clang-format off
uint8_t BluetoothRequestHandler::_dummyAdvertisingServiceUuid[ADVERTISING_SERVICE_UUID_LENGTH] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // First UUID, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xee, 0x00, 0x00, 0x00,
    // Second UUID, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xee, 0x00, 0x00, 0x00,
};
// clang-format on

/**
 * @brief Variable holding the UUID of the primary service (0x2800).
 */
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;

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
        throw MultipleInstancesException();
    }
    else
    {
        _instantiated = true;
    }
}

/**
 * @brief Get this instance of the BluetoothRequestHandler.
 *
 * @return BluetoothRequestHandler* Pointer to this instance of the BluetoothRequestHandler.
 */
BluetoothRequestHandler *BluetoothRequestHandler::GetInstance()
{
    return _instance;
}

/**
 * @brief Get this instance of the BluetoothRequestHandler.
 *
 * @param d Message dispatcher that can be used to send messages to the STM32.
 *
 * @return BluetoothRequestHandler* Pointer to this instance of the BluetoothRequestHandler.
 */
BluetoothRequestHandler *BluetoothRequestHandler::GetInstance(IMessageDispatcher *d)
{
    if (!_instance)
    {
        _instance = new BluetoothRequestHandler(d);
        _instance->Setup();
    }

    return _instance;
}

/**
 *  @brief Create a new BluetoothRequestHandler object.
 *
 *  @param messageDispatcher
 *      Message Dispatcher that can be used to send messages to the STM32.
 */
BluetoothRequestHandler::BluetoothRequestHandler(IMessageDispatcher *messageDispatcher) : BluetoothRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/**
 *  @brief Default destructor for the bluetooth class.
 */
BluetoothRequestHandler::~BluetoothRequestHandler()
{
    //
    //  Production creates this object once and never destroys it, so this only matters to
    //  the tests.  GetInstance() builds a handler only when _instance is null and hands
    //  the pointer out, so a handler that is discarded has to clear it or the next caller
    //  is given a freed object.  The constructor rejects a second instance by throwing
    //  and nothing catches that, so the guard has to go back with it.
    //
    _instance = nullptr;
    _instantiated = false;
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
    TRACE_MESSAGE("Setup: Enter");

    RequestHandlerBase::Setup();

    _xBtEventGroup = xEventGroupCreate();
    xEventGroupSetBits(_xBtEventGroup, BT_READY_BIT);

    TRACE_MESSAGE("BT controller mem release...");

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    xTaskCreate(Task, _taskName, TASK_STACK_SIZE, this, PRIORITY_BLUETOOTH_TASK, &_taskHandle);

    TRACE_MESSAGE("Setup: Exit");
}

/**
 * @brief Generate the advertising data for the device.
 */
void BluetoothRequestHandler::GenerateAdvertisingData()
{
    TRACE_MESSAGE("GenerateAdvertisingData: Enter");

    uint16_t companyIdentifier = _manager.GetCompanyIdentifier();
    _manufacturerData[0] = static_cast<uint8_t>((companyIdentifier & 0x00FF));
    _manufacturerData[1] = static_cast<uint8_t>(((companyIdentifier & 0xFF00) >> 8));

    memset(&_advertisingData, 0, sizeof(esp_ble_adv_data_t));
    /* The length of adv data must be less than 31 bytes */
    _advertisingData.set_scan_rsp = false;
    _advertisingData.include_name = true;
    _advertisingData.include_txpower = true;
    _advertisingData.min_interval = 0x0006; // slave connection min interval, Time = min_interval * 1.25 msec
    _advertisingData.max_interval = 0x0010; // slave connection max interval, Time = max_interval * 1.25 msec
    _advertisingData.appearance = 0x00;
    _advertisingData.manufacturer_len = sizeof(_manufacturerData);
    _advertisingData.p_manufacturer_data = _manufacturerData;
    _advertisingData.service_data_len = 0;
    _advertisingData.p_service_data = NULL;
    _advertisingData.service_uuid_len = sizeof(_dummyAdvertisingServiceUuid);
    _advertisingData.p_service_uuid = _dummyAdvertisingServiceUuid;
    _advertisingData.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    // Scan response data
    memset(&_scanResponseData, 0, sizeof(esp_ble_adv_data_t));
    _scanResponseData.set_scan_rsp = true;
    _scanResponseData.include_name = true;
    _scanResponseData.include_txpower = true;
    _scanResponseData.manufacturer_len = sizeof(_manufacturerData);
    _scanResponseData.p_manufacturer_data = _manufacturerData;

    // Advertising parameters
    memset(&_advertisingParameters, 0, sizeof(esp_ble_adv_params_t));
    _advertisingParameters.adv_int_min = 0x20;
    _advertisingParameters.adv_int_max = 0x40;
    _advertisingParameters.adv_type = ADV_TYPE_IND;
    _advertisingParameters.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    _advertisingParameters.channel_map = ADV_CHNL_ALL;
    _advertisingParameters.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    TRACE_MESSAGE("GenerateAdvertisingData: Exit");
}

/**
 * @brief Get a pointer to the attribute table.
 *
 * @return esp_gatts_attr_db_t* Pointer to the attribute table.
 */
esp_gatts_attr_db_t *BluetoothRequestHandler::GetAttributeTable()
{
    return _attribute_table;
}

/**
 * @brief Get a pointer to the advertising data.
 *
 * @return esp_ble_adv_data_t* Pointer to the advertising data.
 */
esp_ble_adv_data_t *BluetoothRequestHandler::GetAdvertisingData()
{
    return &_advertisingData;
}

/**
 * @brief Get a pointer to the advertising parameters.
 *
 * @return esp_ble_adv_params_t* Pointer to the advertising parameters.
 */
esp_ble_adv_params_t *BluetoothRequestHandler::GetAdvertisingParameters()
{
    return &_advertisingParameters;
}

/**
 * @brief Get a pointer to the scan response data.
 *
 * @return esp_ble_adv_data_t* Pointer the scan response data.
 */
esp_ble_adv_data_t *BluetoothRequestHandler::GetScanResponseData()
{
    return &_scanResponseData;
}

/**
 * @brief Get the length of the attributes table.
 *
 * @return uint16_t Length of the attributes table.
 */
uint16_t BluetoothRequestHandler::GetAttributeTableLength()
{
    return _attributeTableLength;
}

/**
 * @brief Get the number of attributes in the attribute table.
 *
 * @return int Number of handles.
 */
int BluetoothRequestHandler::GetHandleCount()
{
    return _attributeTableLength;
}

/**
 * @brief Generate the attributes table for the GATT database.
 */
void BluetoothRequestHandler::GenerateAttributeTable()
{
    // TODO: how many attributes are we going to need?
    // 1 for each service
    // 2 for each characteristic (declaration + value)
    // 1 for each descriptor

    TRACE_MESSAGE("GenerateAttributeTable: Enter");

    auto count = 0;

    // Not thrilled with the double-iteration but:
    // - the list is small
    // - it's better than dynamic resizing the array for every attribute we need to add
    for (const auto &svc: _manager.GetServiceDefinitions())
    {
        // 1 for the service
        count++;

        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            count += 2; // +2 for the declaration and value

            for (const auto &descriptor: characteristic->GetDescriptorDefinitions())
            {
                (void) descriptor; // Squash the warning about unused variable
                count++;           // +1 for the descriptor
            }
        }
    }

    TRACE_MESSAGE("ATTRIBUTE TABLE LENGTH = %" PRId32, count);

    _attribute_table = new esp_gatts_attr_db_t[count];
    _attributeTableLength = count;

    auto index = 0;
    for (const auto &svc: _manager.GetServiceDefinitions())
    {
        TRACE_MESSAGE("ATTRIBUTE TABLE SERVICE AT %" PRId32, index);
        // service attribute - all services are primary for now
        _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(const_cast<uint16_t *>(&primary_service_uuid)), ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(svc->Uuid), reinterpret_cast<uint8_t *>(&svc->Uuid)}};

        TRACE_MESSAGE("CONTAINS %" PRIu32 " CHARACTERISTICS", svc->GetCharacteristicDefinitions().size());
        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            TRACE_MESSAGE("ATTRIBUTE TABLE DECLARATION AT %" PRId32, index);
            // declaration attribute
            _attribute_table[index++] = {
                {ESP_GATT_AUTO_RSP},
                {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&characteristic->Declaration->Uuid), static_cast<uint16_t>(characteristic->Declaration->Permission), CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, reinterpret_cast<uint8_t *>(&characteristic->Declaration->Properties)}};

            TRACE_MESSAGE("ATTRIBUTE TABLE VALUE AT %" PRId32, index);
            // value attribute
            // the first param here (ESP_GATT_RSP_BY_APP) means that we'll handle the request for data manually rather than allowing the API to respond with
            // data in the parameter database.  We need to call to the STM for data, so this is sensible.
            if (characteristic->Value->Is128())
            {
                _attribute_table[index++] = {
                    {ESP_GATT_RSP_BY_APP},
                    {ESP_UUID_LEN_128, reinterpret_cast<uint8_t *>(&characteristic->Value->uuid128), static_cast<uint16_t>(characteristic->Value->Permission), characteristic->Value->MaxLength, 0, NULL}}; // <- 0, NULL means "current value length is 0, current value data is null"
            }
            else
            {
                _attribute_table[index++] = {
                    {ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&characteristic->Value->uuid16), static_cast<uint16_t>(characteristic->Value->Permission), characteristic->Value->MaxLength, 0, NULL}}; // <- 0, NULL means "current value length is 0, current value data is null"
            }

            for (const auto &descriptor: characteristic->GetDescriptorDefinitions())
            {
                TRACE_MESSAGE("ATTRIBUTE TABLE DESCRIPTOR AT %" PRId32, index);
                if (descriptor->Is128())
                {
                    _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, reinterpret_cast<uint8_t *>(&descriptor->uuid128), static_cast<uint16_t>(descriptor->Permission), descriptor->MaxLength, descriptor->CurrentLength, descriptor->Value}};
                }
                else
                {
                    _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&descriptor->uuid16), static_cast<uint16_t>(descriptor->Permission), descriptor->MaxLength, descriptor->CurrentLength, descriptor->Value}};
                }
            }
        }
    }

    TRACE_MESSAGE("GenerateAttributeTable: Exit");
}

/**
 * @brief Get the handles for the services, characteristics and descriptors.
 *
 * @param request Original request from the STM32.
 */
void BluetoothRequestHandler::GetHandles(Message * const request)
{
    TRACE_MESSAGE("GetHandles: Enter");

    Esp32Messaging::BTGetHandlesResponse response;
    response.HandleCount = GetHandleCount();
    response.HandlesLength = response.HandleCount * 2;
    response.Handles = static_cast<uint8_t *>(pvPortMalloc(response.HandlesLength));
    _manager.GetHandles(reinterpret_cast<uint16_t *>(response.Handles));

    request->PayloadLength = Encoders::EncodedBTGetHandlesResponseBufferSize(&response);
    request->Payload = static_cast<uint8_t *>(pvPortMalloc(request->PayloadLength));
    Encoders::EncodeBTGetHandlesResponse(&response, request->Payload);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&response.Handles));

    request->MessageType = MessageTypes::Response;
    request->StatusCode = StatusCodes::CompletedOk;

    _messageDispatcher->QueueMessageForStm32(request);

    TRACE_MESSAGE("GetHandles: Exit");
}

/**
 * @brief Set the value for the specified attribute handle.
 *
 * @param request Request from the STM32.
 */
void BluetoothRequestHandler::SetCurrentValueForAttributeHandle(Message * const request)
{
    TRACE_MESSAGE("SetCurrentValueForAttributeHandle: Enter");

    Esp32Messaging::BTServerDataSet *ds = Encoders::ExtractBTServerDataSet(request->Payload);
    request->DeletePayload();

    TRACE_MESSAGE("Server wrote %" PRIu32 " bytes to 0x%04" PRIx16, ds->SetDataLength, ds->Handle);

    _manager.SetCurrentValueForAttributeHandle(ds->Handle, ds->SetData, ds->SetDataLength);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (_connectionHandle != INVALID_CONNECTION_HANDLE)
    {
        if ((_manager.GetCharacteristicProperties(ds->Handle) & CharacteristicProperty::Notify) == CharacteristicProperty::Notify)
        {
            if (esp_ble_gatts_send_indicate(_gattsInterfaceHandle, _connectionHandle, ds->Handle, ds->SetDataLength, ds->SetData, false) != ESP_OK)
            {
                TRACE_MESSAGE("Failed to send indication to connection ID 0x%04" PRIx16, _connectionHandle);
                result = StatusCodes::Failure;
            }
        }
    }
    Encoders::FreeBTServerDataSet(&ds);

    request->MessageType = MessageTypes::Response;
    request->StatusCode = result;

    _messageDispatcher->QueueMessageForStm32(request);

    TRACE_MESSAGE("SetCurrentValueForAttributeHandle: Exit");
}

/**
 *  @brief Dispatch bluetooth messages to the appropriate method.
 *
 *  @param request
 *      Bluetooth request being made by the STM32.
 */
void BluetoothRequestHandler::DispatchRequest(Message *request)
{
    TRACE_MESSAGE("DispatchRequest: Enter");

    xEventGroupClearBits(_xBtEventGroup, BT_READY_BIT);
    switch (request->Function)
    {
        case BluetoothFunction::Start:
            Start(request);
            break;
        case BluetoothFunction::ServerDataSet:
            SetCurrentValueForAttributeHandle(request);
            break;
        case BluetoothFunction::GetHandles:
            GetHandles(request);
            break;
        case BluetoothFunction::Stop:
            Stop(request);
            break;
        default:
            TRACE_MESSAGE("Unknown Bluetooth message received, function %" PRIu32, request->Function);
            if (request->Payload != NULL)
            {
                TRACE_HEX_BUFFER(request->Payload, request->PayloadLength);
            }
            break;
    }
    //    xEventGroupWaitBits(_xBtEventGroup, BT_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    TRACE_MESSAGE("DispatchRequest: Exit");
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
    TRACE_MESSAGE("EventHandlerHelper: Enter with base %s and id 0x%" PRIx32, eventBase, static_cast<uint32_t>(eventId));

    TRACE_MESSAGE("EventHandlerHelper: Exit");
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
    TRACE_MESSAGE("EventHandler: Enter");

    BluetoothRequestHandler *sender = static_cast<BluetoothRequestHandler *>(arg);
    sender->EventHandlerHelper(eventBase, eventId, eventData);

    TRACE_MESSAGE("EventHandler: Exit");
}

/**
 * @brief Prepare the Bluetooth interface for deep sleep.
 */
StatusCodes::StatusCodes BluetoothRequestHandler::PrepareForDeepSleep()
{
    TRACE_MESSAGE("PrepareForDeepSleep(): Enter");

    BluetoothRequestHandler *bluetoothHandler = BluetoothRequestHandler::GetInstance();
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (bluetoothHandler != NULL)
    {
        if (!bluetoothHandler->StopBluetoothService())
        {
            result = StatusCodes::Failure;
        }
    }

    TRACE_MESSAGE("PrepareForDeepSleep(): Exit");

    return (result);
}

/**
 * @brief Start the Bluetooth stack.
 *
 * @return true if the system was started, false otherwise.
 */
bool BluetoothRequestHandler::StartBluetoothService()
{
    TRACE_MESSAGE("StartBluetoothService: Enter");

    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    TRACE_MESSAGE("Start: esp_bt_controller_init");
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        TRACE_MESSAGE("%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    TRACE_MESSAGE("Start: esp_bt_controller_enable");
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ERROR_MESSAGE("esp_bt_controller_enable failed, code %" PRIi32 ", %s", ret, esp_err_to_name(ret));
        return false;
    }

    TRACE_MESSAGE("Start: esp_bluedroid_init");
    ret = esp_bluedroid_init();
    if (ret)
    {
        ERROR_MESSAGE("esp_bluedroid_init failed %" PRIi32, ret);
        return false;
    }

    TRACE_MESSAGE("Start: esp_bluedroid_enable");
    ret = esp_bluedroid_enable();
    if (ret)
    {
        ERROR_MESSAGE("%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return false;
    }

    TRACE_MESSAGE("Start: esp_ble_gatts_register_callback");
    ret = esp_ble_gatts_register_callback(this->GATTSProfileEventHandler);
    if (ret)
    {
        ERROR_MESSAGE("gatts register error, error code = %" PRIi32, ret);
        return false;
    }

    TRACE_MESSAGE("Start: esp_ble_gap_register_callback");
    ret = esp_ble_gap_register_callback(this->GAPEventHandler);
    if (ret)
    {
        ERROR_MESSAGE("gap register error, error code = %" PRIi32, ret);
        return false;
    }

    TRACE_MESSAGE("Start: esp_ble_gatts_app_register");
    ret = esp_ble_gatts_app_register(MEADOW_PROFILE_APP_ID);
    if (ret)
    {
        ERROR_MESSAGE("gatts app register error, error code = %" PRIi32, ret);
        return false;
    }

    TRACE_MESSAGE("Start: esp_ble_gatt_set_local_mtu");
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(BT_GATT_MTU);
    if (local_mtu_ret)
    {
        ERROR_MESSAGE("Set local  MTU failed, error code = %" PRIi32, local_mtu_ret);
    }

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND; // bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;                   // set the IO capability to No output No input
    uint8_t key_size = 16;                                      // the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    // set static passkey
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

    TRACE_MESSAGE("Start: esp_ble_gap_set_security_param (ESP_BLE_SM_SET_INIT_KEY)");
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));

    TRACE_MESSAGE("Start: esp_ble_gap_set_security_param (ESP_BLE_SM_SET_RSP_KEY)");
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    TRACE_MESSAGE("StartBluetoothService: Exit");

    return true;
}

/**
 * @brief Teardown the Bluetooth stack by turning off the Bluetooth adapter.
 *
 * @return true if the system was stopped, false otherwise.
 */
bool BluetoothRequestHandler::StopBluetoothService()
{
    TRACE_MESSAGE("StopBluetoothService: Enter");

    TRACE_MESSAGE("Stop: esp_ble_gap_stop_advertising");
    if (esp_ble_gap_stop_advertising() != ESP_OK)
    {
        ERROR_MESSAGE("Failed to stop advertising");
        return false;
    }

    if (_gattsInterfaceHandle != ESP_GATT_IF_NONE)
    {
        TRACE_MESSAGE("Stop: esp_ble_gatts_app_unregister");
        if (esp_ble_gatts_app_unregister(_gattsInterfaceHandle) != ESP_OK)
        {
            ERROR_MESSAGE("Failed to unregister GATTS app");
            return false;
        }
    }

    TRACE_MESSAGE("Stop: esp_bluedroid_disable");
    if (esp_bluedroid_disable() != ESP_OK)
    {
        ERROR_MESSAGE("Failed to disable bluetooth stack");
        return false;
    }

    TRACE_MESSAGE("Stop: esp_bluedroid_deinit");
    if (esp_bluedroid_deinit() != ESP_OK)
    {
        ERROR_MESSAGE("Failed to deinitialise bluetooth stack");
        return false;
    }
    //
    //  This call fails...
    //
    vTaskDelay(500 / portTICK_PERIOD_MS);

    TRACE_MESSAGE("Stop: esp_bt_controller_disable");
    if (esp_bt_controller_disable() != ESP_OK)
    {
        ERROR_MESSAGE("Failed to disable BT controller");
        return false;
    }

    TRACE_MESSAGE("Stop: esp_bt_controller_deinit");
    if (esp_bt_controller_deinit() != ESP_OK)
    {
        ERROR_MESSAGE("Failed to deinitialise BT controller");
        return false;
    }

    TRACE_MESSAGE("StopBluetoothService: Exit");

    return true;
}

/**
 * @brief Stop the Bluetooth service.
 *
 * @param request Request from the STM32.
 */
void BluetoothRequestHandler::Stop(Message * const request)
{
    TRACE_MESSAGE("Stop: Enter");

    RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::BluetoothStoppingEvent, StatusCodes::CompletedOk);

    if (StopBluetoothService())
    {
        request->StatusCode = StatusCodes::CompletedOk;
        if (_manager.DeleteAllServices())
        {
            request->StatusCode = StatusCodes::CompletedOk;
            RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::BluetoothStoppedEvent, StatusCodes::CompletedOk);
        }
        else
        {
            request->StatusCode = StatusCodes::Failure;
        }
    }
    else
    {
        request->StatusCode = StatusCodes::Failure;
    }
    request->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(request);

    xEventGroupSetBits(_xBtEventGroup, BT_READY_BIT);

    TRACE_MESSAGE("Stop: Exit");
}

/**
 * @brief Get the MAC address for the Bluetooth interface.
 *
 * @param macAddress Buffer to hold the MAC address.
 */
void BluetoothRequestHandler::GetMacAddress(uint8_t * const macAddress)
{
    TRACE_MESSAGE("GetMacAddress: Enter");

    if (esp_read_mac(macAddress, ESP_MAC_BT) != ESP_OK)
    {
        memset(macAddress, 0, 6);
    }
    TRACE_MESSAGE("BT MAC address is %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8, macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);

    TRACE_MESSAGE("GetMacAddress: Exit");
}

/**
 * @brief Process a write data request from the Bluetooth client.
 *
 * @param handle Handle for the item that has been changed.
 * @param data Data that has been written.
 * @param dataLength Amount of data that has been written.
 */
void BluetoothRequestHandler::OnDataWriteRequest(const uint16_t handle, uint8_t * const data, const uint16_t dataLength)
{
    TRACE_MESSAGE("OnDataWriteRequest: Enter");

    Esp32Messaging::BTDataWriteRequest *request = static_cast<Esp32Messaging::BTDataWriteRequest *>(pvPortMalloc(sizeof(Esp32Messaging::BTDataWriteRequest)));
    request->Handle = handle;
    request->DataLength = dataLength;
    request->Data = data;

    int payload_length = Encoders::EncodedBTDataWriteRequestBufferSize(request);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payload_length));
    Encoders::EncodeBTDataWriteRequest(request, payload);
    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&request));

    GetInstance()->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, StatusCodes::CompletedOk, payload, payload_length);

    TRACE_MESSAGE("OnDataWriteRequest: Exit");
}

/**
 * @brief Build the service and characteristic graph from the JSON configuration.
 *
 * @param request Message containing the JSON configuration.
 */
void BluetoothRequestHandler::Start(Message * const request)
{
    TRACE_MESSAGE("Start: Enter");

    RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::BluetoothStartingEvent, StatusCodes::CompletedOk);

    Esp32Messaging::BTStackConfig *config = Encoders::ExtractBTStackConfig(request->Payload);
    request->DeletePayload();

    _manager.BuildGraphFromJson(config->Config);
    Encoders::FreeBTStackConfig(&config);

    GenerateAdvertisingData();
    GenerateAttributeTable();
    if (StartBluetoothService())
    {
        request->StatusCode = StatusCodes::CompletedOk;
        RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::BluetoothStartedEvent, StatusCodes::CompletedOk);
    }
    else
    {
        request->StatusCode = StatusCodes::Failure;
    }

    request->MessageType = MessageTypes::Response;

    _messageDispatcher->QueueMessageForStm32(request);

    xEventGroupSetBits(_xBtEventGroup, BT_READY_BIT);

    TRACE_MESSAGE("Start: Exit");
}

/**
 * @brief Process GATTS events.
 *
 * @param event Event ID for the event that has been generated.
 * @param gatts_if GATT server callback.
 * @param param Parameters for the event.
 */
void BluetoothRequestHandler::GATTSProfileEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    TRACE_MESSAGE("GATTSProfileEventHandler: Enter");

    esp_err_t ret = ESP_OK;
    switch (event)
    {
        case ESP_GATTS_REG_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_REG_EVT");

                _gattsInterfaceHandle = gatts_if;
                esp_ble_gap_set_device_name(_manager.GetDeviceName());

                // turn on some security
                //               esp_ble_gap_config_local_privacy(true);

                ret = esp_ble_gap_config_adv_data(GetInstance()->GetAdvertisingData());
                if (ret)
                {
                    ERROR_MESSAGE("Config adv data failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }
                _advertisingDataConfigured = true;

                // configure our BLE scan response
                ret = esp_ble_gap_config_adv_data(GetInstance()->GetScanResponseData());
                if (ret)
                {
                    ERROR_MESSAGE("Config scan response data failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }
                _responseDataConfigured = true;

                TRACE_MESSAGE("BLE advertising configured.  Creating attributes...");

                // set the attribute table - this requires the services be already-defined in the Manager
                esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(GetInstance()->GetAttributeTable(), gatts_if, GetInstance()->GetAttributeTableLength(), BluetoothRequestHandler::INSTANCE_ID);
                if (create_attr_ret)
                {
                    ERROR_MESSAGE("Create attr table failed, error code = %" PRIi32, create_attr_ret);
                }
                else
                {
                    TRACE_MESSAGE("BLE attribute table created");
                }
            }
            break;
        case ESP_GATTS_CREATE_EVT:
            TRACE_MESSAGE("ESP_GATTS_CREATE_EVT");
            break;
        case ESP_GATTS_READ_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_READ_EVT, reading handle 0x%04" PRIx16 " need response=%" PRIi32, param->read.handle, static_cast<int32_t>(param->read.need_rsp));

                if (param->read.need_rsp)
                {
                    // the app needs to build a response - this is (currently) set for Characteristic Values - so call to the manager to get the value for the handle
                    esp_gatt_rsp_t rsp;
                    memset(&rsp, 0, sizeof(esp_gatt_rsp_t));

                    rsp.attr_value.handle = param->read.handle;

                    // go get the currently known value from the manager
                    bool handleFound = _manager.GetCurrentValueForAttributeHandle(param->read.handle, rsp.attr_value.value, &rsp.attr_value.len);

                    TRACE_MESSAGE("  Sending read response of %" PRIu16 " bytes", rsp.attr_value.len);
                    ret = esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, handleFound ? ESP_GATT_OK : ESP_GATT_INVALID_HANDLE, &rsp);

                    if (ret)
                    {
                        ERROR_MESSAGE("esp_ble_gatts_send_response failed, error code = %" PRIi32, ret);
                    }
                }
            }
            break;
        case ESP_GATTS_WRITE_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_WRITE_EVT, write handle 0x%04" PRIx16, param->write.handle);
                if (param->write.need_rsp)
                {
                    OnDataWriteRequest(param->write.handle, param->write.value, param->write.len);

                    ret = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    if (ret)
                    {
                        ERROR_MESSAGE("esp_ble_gatts_send_response failed, error code = %" PRIi32, ret);
                    }
                    else
                    {
                        TRACE_MESSAGE("esp_ble_gatts_send_response succeeded");
                    }
                }
            }
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            TRACE_MESSAGE("ESP_GATTS_EXEC_WRITE_EVT");
            break;
        case ESP_GATTS_MTU_EVT:
            TRACE_MESSAGE("ESP_GATTS_MTU_EVT");
            break;
        case ESP_GATTS_CONF_EVT:
            TRACE_MESSAGE("ESP_GATTS_CONF_EVT");
            break;
        case ESP_GATTS_UNREG_EVT:
            TRACE_MESSAGE("ESP_GATTS_UNREG_EVT");
            // if (esp_ble_gatts_stop_service(GetInstance()->_serviceHandle) != ESP_OK)
            // {
            //     ERROR_MESSAGE("Failed to stop service");
            // }
            // GetInstance()->_serviceHandle = -1;
            break;
        case ESP_GATTS_DELETE_EVT:
            TRACE_MESSAGE("ESP_GATTS_DELETE_EVT");

            break;
        case ESP_GATTS_START_EVT:
            TRACE_MESSAGE("ESP_GATTS_START_EVT");
            break;
        case ESP_GATTS_STOP_EVT:
            TRACE_MESSAGE("ESP_GATTS_STOP_EVT");
            break;
        case ESP_GATTS_RESPONSE_EVT:
            TRACE_MESSAGE("ESP_GATTS_RESPONSE_EVT");
            break;
        case ESP_GATTS_CONNECT_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_CONNECT_EVT");
                /* start security connect with peer device when receive the connect event sent by the master */
                //              esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);

                TRACE_MESSAGE("    Connection ID 0x%04" PRIx16, param->connect.conn_id);
                _connectionHandle = param->connect.conn_id;
                esp_ble_conn_update_params_t conn_params;
                memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
                /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
                conn_params.latency = 0;
                conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
                conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
                conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
                // start sent the update connection parameters to the peer device.
                esp_ble_gap_update_conn_params(&conn_params);
            }
            GetInstance()->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientConnectedEvent, StatusCodes::CompletedOk);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            TRACE_MESSAGE("ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%" PRIx32, param->disconnect.reason);
            /* start advertising again when missing the connect */
            esp_ble_gap_start_advertising(GetInstance()->GetAdvertisingParameters());
            _connectionHandle = 0xffff;
            GetInstance()->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
            break;
        case ESP_GATTS_OPEN_EVT:
            TRACE_MESSAGE("ESP_GATTS_OPEN_EVT");
            break;
        case ESP_GATTS_CANCEL_OPEN_EVT:
            TRACE_MESSAGE("ESP_GATTS_CANCEL_OPEN_EVT");
            break;
        case ESP_GATTS_CLOSE_EVT:
            TRACE_MESSAGE("ESP_GATTS_CLOSE_EVT");
            break;
        case ESP_GATTS_LISTEN_EVT:
            TRACE_MESSAGE("ESP_GATTS_LISTEN_EVT");
            break;
        case ESP_GATTS_CONGEST_EVT:
            TRACE_MESSAGE("ESP_GATTS_CONGEST_EVT");
            break;
        case ESP_GATTS_SET_ATTR_VAL_EVT:
            TRACE_MESSAGE("ESP_GATTS_SET_ATTR_VAL_EVT");
            break;
        case ESP_GATTS_SEND_SERVICE_CHANGE_EVT:
            TRACE_MESSAGE("ESP_GATTS_SEND_SERVICE_CHANGE_EVT");
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            TRACE_MESSAGE("ESP_GATTS_CREAT_ATTR_TAB_EVT");
            if (param->add_attr_tab.status != ESP_GATT_OK)
            {
                ERROR_MESSAGE("Create attribute table failed, error code=0x%" PRIx32, param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != GetInstance()->GetAttributeTableLength())
            {
                ERROR_MESSAGE("Create attribute table failed, num_handle (%" PRIu16 ") does not match the expected attribute table length (%" PRIu16 ")", param->add_attr_tab.num_handle, GetInstance()->GetAttributeTableLength());
            }
            else
            {
                TRACE_MESSAGE("Create attribute table successfully, handle count = %" PRIu16, param->add_attr_tab.num_handle);
                // the assigned handles with be in param->add_attr_tab.handles - copy them off
                _manager.SetHandles(param->add_attr_tab.handles, param->add_attr_tab.num_handle);
                // now fire up the service (which is the first attribute in the attribute table) by handle.
                // TODO: right now we're cheating
                TRACE_MESSAGE("Starting service with handle 0x%04" PRIx16, param->add_attr_tab.handles[0]);
                GetInstance()->_serviceHandle = param->add_attr_tab.handles[0];
                esp_ble_gatts_start_service(param->add_attr_tab.handles[0]);
            }
            break;
        default:
            TRACE_MESSAGE("Unhandled GATTS event, ID: 0x%08x", event);
            break;
    }

    TRACE_MESSAGE("GATTSProfileEventHandler: Exit");
}

/**
 * @brief GAPE event handler.
 *
 * @param event Event Id for the event that has been generated.
 * @param param Data for the event.
 */
void BluetoothRequestHandler::GAPEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    TRACE_MESSAGE("GAPEventHandler: Enter");

    switch (event)
    {
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: // 1
            TRACE_MESSAGE("ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT");
            _responseDataConfigured = false;
            if (!_advertisingDataConfigured)
            {
                TRACE_MESSAGE("Scan data set.  Starting advertising...");
                esp_ble_gap_start_advertising(GetInstance()->GetAdvertisingParameters());
            }
            break;
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: // 0
            TRACE_MESSAGE("ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT");
            // triggered by the gatts_event_handler for ESP_GATTS_REG_EVT where we set the advertising data
            _advertisingDataConfigured = false;
            if (!_responseDataConfigured)
            {
                TRACE_MESSAGE("Advertising data set.  Starting advertising...");
                esp_ble_gap_start_advertising(GetInstance()->GetAdvertisingParameters());
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: // 6
            TRACE_MESSAGE("ESP_GAP_BLE_ADV_START_COMPLETE_EVT");
            // advertising has started (well, unless an error is set, so check for that)
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
            {
                ERROR_MESSAGE("Advertising start failed");
            }
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT: // 12
            /* passkey request event */
            TRACE_MESSAGE("ESP_GAP_BLE_PASSKEY_REQ_EVT");
            /* Call the following function to input the passkey which is displayed on the remote device */
            // esp_ble_passkey_reply(heart_rate_profile_tab[HEART_PROFILE_APP_IDX].remote_bda, true, 0x00);
            break;
        case ESP_GAP_BLE_OOB_REQ_EVT: // 13
            {
                TRACE_MESSAGE("ESP_GAP_BLE_OOB_REQ_EVT");
                uint8_t tk[16] = {1}; // If you paired with OOB, both devices need to use the same tk
                esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
            }
            break;
        case ESP_GAP_BLE_LOCAL_IR_EVT: /* BLE local IR event */
            TRACE_MESSAGE("ESP_GAP_BLE_LOCAL_IR_EVT");
            break;
        case ESP_GAP_BLE_LOCAL_ER_EVT: /* BLE local ER event */
            TRACE_MESSAGE("ESP_GAP_BLE_LOCAL_ER_EVT");
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:   // 16
            /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
            show the passkey number to the user to confirm it with the number displayed by peer device. */
            esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
            TRACE_MESSAGE("ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%" PRIu32, param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT: // 10
            TRACE_MESSAGE("ESP_GAP_BLE_SEC_REQ_EVT");
            // security request
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: // 11
            /// the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
            /// show the passkey number to the user to input it in the peer device.
            TRACE_MESSAGE("ESP_GAP_BLE_PASSKEY_NOTIF_EVT passkey Notify number:%" PRIu32, param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_KEY_EVT: // 9

            // shows the ble key info share with peer device to the user.
            TRACE_MESSAGE("ESP_GAP_BLE_KEY_EVT key type = %" PRId32, param->ble_security.ble_key.key_type);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT: // 8
            TRACE_MESSAGE("ESP_GAP_BLE_AUTH_CMPL_EVT");
            // auth complete
            if (param->ble_security.auth_cmpl.success)
            {
                TRACE_MESSAGE("Auth success. mode = %" PRId32, param->ble_security.auth_cmpl.auth_mode);
            }
            else
            {
                ERROR_MESSAGE("Auth failed. reason = 0x%" PRIx32, param->ble_security.auth_cmpl.fail_reason);
            }
            break;
        case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT: // 23
            DEBUG_MESSAGE("ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT status = %" PRId32, param->remove_bond_dev_cmpl.status);
            TRACE_MESSAGE("ESP_GAP_BLE_REMOVE_BOND_DEV");
            TRACE_MESSAGE("-----ESP_GAP_BLE_REMOVE_BOND_DEV----");
            TRACE_HEX_BUFFER(param->remove_bond_dev_cmpl.bd_addr, sizeof(esp_bd_addr_t));
            TRACE_MESSAGE("------------------------------------");
            break;
        case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT: // 22
            {
                TRACE_MESSAGE("ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT");
                if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS)
                {
                    ERROR_MESSAGE("Config local privacy failed, error status = %" PRId32, param->local_privacy_cmpl.status);
                    break;
                }

                TRACE_MESSAGE("GAP 22: local privacy set. Configuring advertising");
                esp_err_t ret = esp_ble_gap_config_adv_data(GetInstance()->GetAdvertisingData());
                if (ret)
                {
                    ERROR_MESSAGE("Config adv data failed, error code = %" PRIi32, ret);
                }
                else
                {
                    _advertisingDataConfigured = true;
                }

                ret = esp_ble_gap_config_adv_data(GetInstance()->GetScanResponseData());
                if (ret)
                {
                    ERROR_MESSAGE("Config adv data failed, error code = %" PRIi32, ret);
                }
                else
                {
                    _responseDataConfigured = true;
                }
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: // 20
            TRACE_MESSAGE("ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: GAP connection parameters successfully changed");
            break;
        case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
            TRACE_MESSAGE("ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT");
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: // 7
            TRACE_MESSAGE("ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT");
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
            {
                ERROR_MESSAGE("Advertising stop failed");
            }
            else
            {
                TRACE_MESSAGE("Stopped advertising successfully");
            }
            break;
        default:
            TRACE_MESSAGE("Unhandled GAP event, ID: 0x%08" PRIx32, event);
            break;
    }

    TRACE_MESSAGE("GAPEventHandler: Exit");
}