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
std::atomic<uint16_t> BluetoothRequestHandler::_connectionHandle(INVALID_CONNECTION_HANDLE);
std::atomic<uint32_t> BluetoothRequestHandler::_notificationsSent(0);
uint8_t *BluetoothRequestHandler::_preparedWriteValue = nullptr;
uint16_t BluetoothRequestHandler::_preparedWriteHandle = 0;
uint16_t BluetoothRequestHandler::_preparedWriteLength = 0;

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
 * @brief The Bluetooth base UUID, 0000xxxx-0000-1000-8000-00805F9B34FB, least significant
 *        byte first.  A 16-bit UUID xxxx occupies bytes 12 and 13.
 */
static const uint8_t BLUETOOTH_BASE_UUID[ESP_UUID_LEN_128] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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
    delete[] _attribute_table;
    _attribute_table = nullptr;

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

    _xBtEventGroup = Utils::RequireCreated(xEventGroupCreate(), "the Bluetooth event group");

    TRACE_MESSAGE("BT controller mem release...");

    //
    //  Only gives back the memory the unused classic Bluetooth controller holds, so Bluetooth
    //  works without it and a failure is not worth resetting the board for (issue #248).
    //
    const esp_err_t memoryRelease = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (memoryRelease != ESP_OK)
    {
        ESP_LOGE(COMPONENT_NAME, "Setup: could not release the classic Bluetooth controller memory, error %" PRIi32 " (%s)", static_cast<int32_t>(memoryRelease), esp_err_to_name(memoryRelease));
    }

    Utils::RequireTaskCreated(xTaskCreate(Task, _taskName, TASK_STACK_SIZE, this, PRIORITY_BLUETOOTH_TASK, &_taskHandle), "the Bluetooth task");

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

    //
    //  The advertising packet and the scan response are 31 bytes each (issue #345).  The
    //  advertising packet carries the flags (3 bytes), TX power (3), the connection interval
    //  range (6), the company identifier (4) and the configured service's 16-bit UUID (4),
    //  20 bytes in all.  The name, the one field whose length the configuration chooses,
    //  goes in the scan response on its own, which leaves it the most room.
    //
    static_assert((BluetoothManager::MAXIMUM_ADVERTISED_NAME_LENGTH + 2) <= MAXIMUM_ADVERTISING_DATA_LENGTH, "The longest device name must fit in the scan response.");
    memset(_advertisingServiceUuid, 0, sizeof(_advertisingServiceUuid));
    uint16_t advertisedUuidLength = 0;
    const std::vector<BluetoothService *> services = _manager.Services.GetServiceDefinitions();
    if (!services.empty())
    {
        const uint16_t serviceUuid = services.front()->Uuid;
        memcpy(_advertisingServiceUuid, BLUETOOTH_BASE_UUID, sizeof(_advertisingServiceUuid));
        _advertisingServiceUuid[12] = static_cast<uint8_t>(serviceUuid & 0x00ff);
        _advertisingServiceUuid[13] = static_cast<uint8_t>((serviceUuid & 0xff00) >> 8);
        advertisedUuidLength = sizeof(_advertisingServiceUuid);
    }

    memset(&_advertisingData, 0, sizeof(esp_ble_adv_data_t));
    _advertisingData.set_scan_rsp = false;
    _advertisingData.include_name = false;
    _advertisingData.include_txpower = true;
    _advertisingData.min_interval = 0x0006; // slave connection min interval, Time = min_interval * 1.25 msec
    _advertisingData.max_interval = 0x0010; // slave connection max interval, Time = max_interval * 1.25 msec
    _advertisingData.appearance = 0x00;
    _advertisingData.manufacturer_len = sizeof(_manufacturerData);
    _advertisingData.p_manufacturer_data = _manufacturerData;
    _advertisingData.service_data_len = 0;
    _advertisingData.p_service_data = nullptr;
    _advertisingData.service_uuid_len = advertisedUuidLength;
    _advertisingData.p_service_uuid = (advertisedUuidLength == 0) ? nullptr : _advertisingServiceUuid;
    _advertisingData.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    // Scan response data
    memset(&_scanResponseData, 0, sizeof(esp_ble_adv_data_t));
    _scanResponseData.set_scan_rsp = true;
    _scanResponseData.include_name = true;

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

    //
    //  A restart calls this again without ever destroying the handler, so whatever the
    //  previous call allocated has to be released here before it is replaced.
    //
    delete[] _attribute_table;
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
                {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&characteristic->GetDeclaration().Uuid), static_cast<uint16_t>(characteristic->GetDeclaration().Permission), CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, reinterpret_cast<uint8_t *>(&characteristic->GetDeclaration().Properties)}};

            TRACE_MESSAGE("ATTRIBUTE TABLE VALUE AT %" PRId32, index);
            // value attribute
            // the first param here (ESP_GATT_RSP_BY_APP) means that we'll handle the request for data manually rather than allowing the API to respond with
            // data in the parameter database.  We need to call to the STM for data, so this is sensible.
            if (characteristic->GetValue().Is128())
            {
                _attribute_table[index++] = {
                    {ESP_GATT_RSP_BY_APP},
                    {ESP_UUID_LEN_128, reinterpret_cast<uint8_t *>(&characteristic->GetValue().uuid128), static_cast<uint16_t>(characteristic->GetValue().Permission), characteristic->GetValue().MaxLength, 0, nullptr}}; // <- 0, nullptr means "current value length is 0, current value data is null"
            }
            else
            {
                _attribute_table[index++] = {
                    {ESP_GATT_RSP_BY_APP},
                    {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&characteristic->GetValue().uuid16), static_cast<uint16_t>(characteristic->GetValue().Permission), characteristic->GetValue().MaxLength, 0, nullptr}}; // <- 0, nullptr means "current value length is 0, current value data is null"
            }

            for (const auto &descriptor: characteristic->GetDescriptorDefinitions())
            {
                TRACE_MESSAGE("ATTRIBUTE TABLE DESCRIPTOR AT %" PRId32, index);
                if (descriptor->Is128())
                {
                    _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, reinterpret_cast<uint8_t *>(&descriptor->uuid128), static_cast<uint16_t>(descriptor->Permission), descriptor->MaxLength, descriptor->CurrentLength, const_cast<uint8_t *>(descriptor->GetValue())}};
                }
                else
                {
                    _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&descriptor->uuid16), static_cast<uint16_t>(descriptor->Permission), descriptor->MaxLength, descriptor->CurrentLength, const_cast<uint8_t *>(descriptor->GetValue())}};
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

    //
    //  With no handles to report, pvPortMalloc(0) may return nullptr without memory having
    //  run out, so only a buffer that was needed and not obtained is a failure.
    //
    if ((response.HandlesLength > 0) && (response.Handles == nullptr))
    {
        ERROR_MESSAGE("GetHandles: cannot allocate %" PRIu32 " bytes for the handles", static_cast<uint32_t>(response.HandlesLength));
        request->StatusCode = StatusCodes::EspOutOfMemory;
    }
    else
    {
        if (_manager.GetHandles(reinterpret_cast<uint16_t *>(response.Handles), response.HandleCount))
        {
            request->PayloadLength = Encoders::EncodedBTGetHandlesResponseBufferSize(&response);
            request->Payload = static_cast<uint8_t *>(pvPortMalloc(request->PayloadLength));
            if (request->Payload == nullptr)
            {
                request->PayloadLength = 0;
                request->StatusCode = StatusCodes::EspOutOfMemory;
            }
            else
            {
                Encoders::EncodeBTGetHandlesResponse(&response, request->Payload);
                request->StatusCode = StatusCodes::CompletedOk;
            }
        }
        else
        {
            //
            //  The service graph needed more handles than GetHandleCount() predicted, so
            //  response.Handles was only partially filled.  Report the failure rather than
            //  send a payload built from it (issue #350).
            //
            ERROR_MESSAGE("GetHandles: the service graph needed more than %" PRIu16 " handles", response.HandleCount);
            request->StatusCode = StatusCodes::Failure;
        }
    }
    Utils::DeleteAndNullifyPointer(response.Handles);

    request->MessageType = MessageTypes::Response;

    _messageDispatcher->QueueMessageForStm32(request);

    TRACE_MESSAGE("GetHandles: Exit");
}

/**
 * @brief Has the connected client enabled notifications for the characteristic with this value handle?
 *
 *  The stack keeps the value the client wrote to the characteristic's Client Characteristic
 *  Configuration descriptor, which this reads back.
 *
 * @param valueHandle Handle of the characteristic's value.
 *
 * @return true if the client set the notification bit.
 */
bool BluetoothRequestHandler::NotificationsEnabled(const uint16_t valueHandle)
{
    const uint16_t configurationHandle = _manager.GetClientConfigurationHandle(valueHandle);
    if (configurationHandle == 0)
    {
        return (false);
    }

    uint16_t length = 0;
    const uint8_t *value = nullptr;
    if ((esp_ble_gatts_get_attr_value(configurationHandle, &length, &value) != ESP_GATT_OK) || (value == nullptr) || (length < CLIENT_CHARACTERISTIC_CONFIGURATION_LENGTH))
    {
        return (false);
    }
    return (BluetoothManager::ClientConfigurationEnablesNotifications(static_cast<uint16_t>(value[0] | (value[1] << 8))));
}

/**
 * @brief Set every client configuration back to 0, no notifications or indications.
 *
 *  A client that is not bonded starts each connection unsubscribed.  The stack holds one
 *  value for each descriptor, so without this a client that connects after another one
 *  subscribed would be sent notifications it never asked for (issue #572).
 */
void BluetoothRequestHandler::ResetClientConfigurations()
{
    const uint8_t unsubscribed[CLIENT_CHARACTERISTIC_CONFIGURATION_LENGTH] = {0, 0};
    for (const uint16_t handle: _manager.GetClientConfigurationHandles())
    {
        if (esp_ble_gatts_set_attr_value(handle, sizeof(unsubscribed), unsubscribed) != ESP_OK)
        {
            ERROR_MESSAGE("Could not reset the client configuration at handle 0x%04" PRIx16, handle);
        }
    }
}

/**
 * @brief Set the value for the specified attribute handle.
 *
 *  SetDataLength is a uint32_t from the wire, but esp_ble_gatts_send_indicate's length
 *  parameter is uint16_t, so a length above UINT16_MAX would otherwise be silently
 *  truncated and a different amount of data sent than the caller asked for.  The handle
 *  is checked against the attribute table before either the manager or the BLE stack
 *  sees it, rather than relying on the properties lookup incidentally returning nothing
 *  useful for one it does not recognise (issue #300).
 *
 * @param request Request from the STM32.
 */
void BluetoothRequestHandler::SetCurrentValueForAttributeHandle(Message * const request)
{
    TRACE_MESSAGE("SetCurrentValueForAttributeHandle: Enter");

    //
    //  A request with no payload is refused as invalid.  ExtractBTServerDataSet also returns
    //  nullptr when the payload is too short for what it declares, which is invalid too, and
    //  when it cannot allocate.
    //
    const bool hasPayload = (request->Payload != nullptr);
    bool malformed = false;
    Esp32Messaging::BTServerDataSet *ds = hasPayload ? Encoders::ExtractBTServerDataSet(request->Payload, request->PayloadLength, &malformed) : nullptr;
    request->DeletePayload();

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (ds == nullptr)
    {
        if (hasPayload)
        {
            result = malformed ? StatusCodes::InvalidRequest : StatusCodes::EspOutOfMemory;
        }
        else
        {
            ERROR_MESSAGE("SetCurrentValueForAttributeHandle: the request carries no payload.");
            result = StatusCodes::InvalidRequest;
        }
    }
    else
    {
        TRACE_MESSAGE("Server wrote %" PRIu32 " bytes to 0x%04" PRIx16, ds->SetDataLength, ds->Handle);

        if (!_manager.IsKnownCharacteristicHandle(ds->Handle))
        {
            ERROR_MESSAGE("SetCurrentValueForAttributeHandle: unknown handle 0x%04" PRIx16, ds->Handle);
            result = StatusCodes::InvalidRequest;
        }
        else
        {
            if (ds->SetDataLength > UINT16_MAX)
            {
                ERROR_MESSAGE("SetCurrentValueForAttributeHandle: length %" PRIu32 " exceeds the maximum of %u bytes.", ds->SetDataLength, UINT16_MAX);
                result = StatusCodes::InvalidRequest;
            }
            else
            {
                _manager.SetCurrentValueForAttributeHandle(ds->Handle, ds->SetData, static_cast<uint16_t>(ds->SetDataLength));
                //
                //  Read once: a disconnect on the Bluedroid task can change it between a check
                //  and a later read, sending the notification to INVALID_CONNECTION_HANDLE.
                //
                const uint16_t connectionHandle = _connectionHandle.load();
                if (connectionHandle != INVALID_CONNECTION_HANDLE)
                {
                    //
                    //  The value is stored above whatever the client has asked for, so it can
                    //  still read it.  A notification goes only to a client that has enabled
                    //  them in the characteristic's client configuration (issue #572).
                    //
                    if (((_manager.GetCharacteristicProperties(ds->Handle) & CharacteristicProperty::Notify) == CharacteristicProperty::Notify) && NotificationsEnabled(ds->Handle))
                    {
                        if (esp_ble_gatts_send_indicate(_gattsInterfaceHandle, connectionHandle, ds->Handle, static_cast<uint16_t>(ds->SetDataLength), ds->SetData, false) != ESP_OK)
                        {
                            TRACE_MESSAGE("Failed to send indication to connection ID 0x%04" PRIx16, connectionHandle);
                            result = StatusCodes::Failure;
                        }
                        else
                        {
                            _notificationsSent++;
                        }
                    }
                }
            }
        }
        Encoders::FreeBTServerDataSet(&ds);
    }

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
            if (request->Payload != nullptr)
            {
                TRACE_HEX_BUFFER(request->Payload, request->PayloadLength);
            }
            RespondToUnknownFunction(request);
            break;
    }

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
    if (bluetoothHandler != nullptr)
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
 *  A stage that fails undoes the stages before it, in reverse order, so that the stack is
 *  left down and a later Start can try again.  Left up, the controller refuses to be
 *  initialised a second time and its memory stays allocated (issue #296).
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
        TearDownStack();
        return false;
    }

    TRACE_MESSAGE("Start: esp_bt_controller_enable");
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ERROR_MESSAGE("esp_bt_controller_enable failed, code %" PRIi32 ", %s", ret, esp_err_to_name(ret));
        TearDownStack();
        return false;
    }

    TRACE_MESSAGE("Start: esp_bluedroid_init");
    ret = esp_bluedroid_init();
    if (ret)
    {
        ERROR_MESSAGE("esp_bluedroid_init failed %" PRIi32, ret);
        TearDownStack();
        return false;
    }

    TRACE_MESSAGE("Start: esp_bluedroid_enable");
    ret = esp_bluedroid_enable();
    if (ret)
    {
        ERROR_MESSAGE("%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        TearDownStack();
        return false;
    }

    TRACE_MESSAGE("Start: esp_ble_gatts_register_callback");
    ret = esp_ble_gatts_register_callback(this->GATTSProfileEventHandler);
    if (ret)
    {
        ERROR_MESSAGE("gatts register error, error code = %" PRIi32, ret);
        TearDownStack();
        return false;
    }

    TRACE_MESSAGE("Start: esp_ble_gap_register_callback");
    ret = esp_ble_gap_register_callback(this->GAPEventHandler);
    if (ret)
    {
        ERROR_MESSAGE("gap register error, error code = %" PRIi32, ret);
        TearDownStack();
        return false;
    }

    TRACE_MESSAGE("Start: esp_ble_gatts_app_register");
    ret = esp_ble_gatts_app_register(MEADOW_PROFILE_APP_ID);
    if (ret)
    {
        ERROR_MESSAGE("gatts app register error, error code = %" PRIi32, ret);
        TearDownStack();
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
 * @brief Tell Start() whether the asynchronous part of starting the service succeeded.
 *
 * Called from the GATT event handler, which runs on the Bluetooth stack's task, while
 * Start() waits on the Bluetooth request handler's task.
 *
 * @param started true once the service has started, false if a step on the way failed.
 */
void BluetoothRequestHandler::SignalServiceStart(const bool started)
{
    xEventGroupSetBits(_xBtEventGroup, started ? BT_SERVICE_STARTED_BIT : BT_SERVICE_START_FAILED_BIT);
}

/**
 * @brief Take down whatever part of the Bluetooth stack is up, in the reverse of the order it came up in.
 *
 *  Each stage is undone only if the stack says it was reached, so this can be used on a stack that
 *  was only partly started as well as on one that is fully running.  It carries on after a stage
 *  fails, so that one refusal does not leave the stages below it up as well.
 *
 * @return true if every stage that was up has been taken down, false if one refused.
 */
bool BluetoothRequestHandler::TearDownStack()
{
    bool tornDown = true;

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED)
    {
        //
        //  Disabling the stack stops advertising anyway, so failing to ask is not a reason to leave it up.
        //
        TRACE_MESSAGE("TearDownStack: esp_ble_gap_stop_advertising");
        if (esp_ble_gap_stop_advertising() != ESP_OK)
        {
            ERROR_MESSAGE("Failed to stop advertising");
        }

        if (_gattsInterfaceHandle != ESP_GATT_IF_NONE)
        {
            TRACE_MESSAGE("TearDownStack: esp_ble_gatts_app_unregister");
            if (esp_ble_gatts_app_unregister(_gattsInterfaceHandle) != ESP_OK)
            {
                ERROR_MESSAGE("Failed to unregister GATTS app");
            }
            _gattsInterfaceHandle = ESP_GATT_IF_NONE;
        }

        TRACE_MESSAGE("TearDownStack: esp_bluedroid_disable");
        if (esp_bluedroid_disable() != ESP_OK)
        {
            ERROR_MESSAGE("Failed to disable bluetooth stack");
            tornDown = false;
        }
    }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED)
    {
        TRACE_MESSAGE("TearDownStack: esp_bluedroid_deinit");
        if (esp_bluedroid_deinit() != ESP_OK)
        {
            ERROR_MESSAGE("Failed to deinitialise bluetooth stack");
            tornDown = false;
        }
    }

    //
    //  In the past we have seen some failures during teardown and a delay here seemed to help.
    //  Current testing indicates that the delay is not needed.  This comment remains for future
    //  reference in case it is needed again.
    //

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
    {
        TRACE_MESSAGE("TearDownStack: esp_bt_controller_disable");
        if (esp_bt_controller_disable() != ESP_OK)
        {
            ERROR_MESSAGE("Failed to disable BT controller");
            tornDown = false;
        }
    }

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED)
    {
        TRACE_MESSAGE("TearDownStack: esp_bt_controller_deinit");
        if (esp_bt_controller_deinit() != ESP_OK)
        {
            ERROR_MESSAGE("Failed to deinitialise BT controller");
            tornDown = false;
        }
    }

    return (tornDown);
}

/**
 * @brief Teardown the Bluetooth stack by turning off the Bluetooth adapter.
 *
 *  This also takes down a stack that was only partly started, which is how a Start that failed
 *  or timed out is recovered from (issue #296).
 *
 * @return true if the system was stopped, false otherwise.
 */
bool BluetoothRequestHandler::StopBluetoothService()
{
    TRACE_MESSAGE("StopBluetoothService: Enter");

    if ((esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) && (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE))
    {
        ERROR_MESSAGE("Stop: the Bluetooth stack is not running.");
        return false;
    }

    const bool stopped = TearDownStack();

    TRACE_MESSAGE("StopBluetoothService: Exit");

    return (stopped);
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
    if (request == nullptr)
    {
        ERROR_MESSAGE("OnDataWriteRequest: Failed to allocate memory for the write request.");
        return;
    }
    request->Handle = handle;
    request->DataLength = dataLength;
    request->Data = data;

    int payload_length = Encoders::EncodedBTDataWriteRequestBufferSize(request);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payload_length));
    if (payload == nullptr)
    {
        ERROR_MESSAGE("OnDataWriteRequest: Failed to allocate memory for the event payload.");
        Utils::DeleteAndNullifyPointer(request);
        return;
    }
    Encoders::EncodeBTDataWriteRequest(request, payload);
    Utils::DeleteAndNullifyPointer(request);

    GetInstance()->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, StatusCodes::CompletedOk, payload, payload_length);

    TRACE_MESSAGE("OnDataWriteRequest: Exit");
}

/**
 * @brief Add one piece of a prepared (long) write to the value being collected.
 *
 *  Called on the Bluedroid task only, as are ExecuteWrite and DiscardPreparedWrite, so the
 *  prepared write state needs no lock.
 *
 * @param handle Attribute being written.
 * @param offset Offset of this piece in the value.
 * @param value Bytes of this piece.
 * @param length Number of bytes in this piece.
 *
 * @return ESP_GATT_OK if the piece was kept, otherwise the status to send the client.
 */
esp_gatt_status_t BluetoothRequestHandler::PrepareWrite(const uint16_t handle, const uint16_t offset, const uint8_t * const value, const uint16_t length)
{
    if (offset > ESP_GATT_MAX_ATTR_LEN)
    {
        return (ESP_GATT_INVALID_OFFSET);
    }
    if ((static_cast<uint32_t>(offset) + length) > ESP_GATT_MAX_ATTR_LEN)
    {
        return (ESP_GATT_INVALID_ATTR_LEN);
    }
    if ((_preparedWriteValue != nullptr) && (handle != _preparedWriteHandle))
    {
        return (ESP_GATT_PREPARE_Q_FULL);
    }
    if (_preparedWriteValue == nullptr)
    {
        _preparedWriteValue = static_cast<uint8_t *>(pvPortMalloc(ESP_GATT_MAX_ATTR_LEN));
        if (_preparedWriteValue == nullptr)
        {
            return (ESP_GATT_NO_RESOURCES);
        }
        memset(_preparedWriteValue, 0, ESP_GATT_MAX_ATTR_LEN);
        _preparedWriteHandle = handle;
        _preparedWriteLength = 0;
    }
    if (length > 0)
    {
        memcpy(_preparedWriteValue + offset, value, length);
    }
    if ((offset + length) > _preparedWriteLength)
    {
        _preparedWriteLength = offset + length;
    }
    return (ESP_GATT_OK);
}

/**
 * @brief Finish a prepared write: deliver the value collected to the STM32, or discard it.
 *
 * @param execute true to deliver the value, false if the client cancelled the write.
 */
void BluetoothRequestHandler::ExecuteWrite(const bool execute)
{
    if (execute && (_preparedWriteValue != nullptr))
    {
        OnDataWriteRequest(_preparedWriteHandle, _preparedWriteValue, _preparedWriteLength);
    }
    DiscardPreparedWrite();
}

/**
 * @brief Discard any prepared write that has not been executed.
 */
void BluetoothRequestHandler::DiscardPreparedWrite()
{
    Utils::DeleteAndNullifyPointer(_preparedWriteValue);
    _preparedWriteHandle = 0;
    _preparedWriteLength = 0;
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

    //
    //  A second Start while the stack is up would rebuild the service graph, releasing the
    //  value buffers, while the Bluedroid task may be reading them to answer a client (issue
    //  #168).  The stack stays up after a Start that failed or timed out too, so this is
    //  checked against the stack itself rather than a flag of our own.  Stop first.
    //
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_UNINITIALIZED)
    {
        ERROR_MESSAGE("Start: the Bluetooth stack is already running; stop it first.");
        request->DeletePayload();
        request->StatusCode = StatusCodes::BluetoothAlreadyStarted;
        request->MessageType = MessageTypes::Response;
        _messageDispatcher->QueueMessageForStm32(request);
        TRACE_MESSAGE("Start: Exit");
        return;
    }

    //
    //  The extractor returns nullptr for a payload that is missing, one whose configuration is
    //  not terminated within it, and when it cannot allocate, so all three are dealt with here
    //  before anything is used.
    //
    const bool hasPayload = (request->Payload != nullptr);
    bool configMalformed = false;
    Esp32Messaging::BTStackConfig *config = hasPayload ? Encoders::ExtractBTStackConfig(request->Payload, request->PayloadLength, &configMalformed) : nullptr;
    request->DeletePayload();

    bool graphBuilt = false;
    StatusCodes::StatusCodes refusal = StatusCodes::InvalidConfigurationFile;
    if (config == nullptr)
    {
        if (!hasPayload)
        {
            ERROR_MESSAGE("Start: the request carries no configuration.");
        }
        else
        {
            if (configMalformed)
            {
                ERROR_MESSAGE("Start: the configuration is not terminated within the payload.");
            }
            else
            {
                ERROR_MESSAGE("Start: could not allocate the stack configuration.");
                refusal = StatusCodes::EspOutOfMemory;
            }
        }
    }
    else
    {
        graphBuilt = _manager.BuildGraphFromJson(config->Config);
        Encoders::FreeBTStackConfig(&config);
    }

    if (!graphBuilt)
    {
        request->StatusCode = refusal;
    }
    else
    {
        GenerateAdvertisingData();
        GenerateAttributeTable();
        xEventGroupClearBits(_xBtEventGroup, BT_SERVICE_STARTED_BIT | BT_SERVICE_START_FAILED_BIT);
        if (StartBluetoothService())
        {
            //
            //  StartBluetoothService() only makes the synchronous calls.  The GATT application
            //  is registered, the attribute table created and the service started later, in
            //  the GATT event handler on the Bluetooth stack's own task, so wait for it to
            //  report how that went before telling the STM32 anything.
            //
            const EventBits_t bits = xEventGroupWaitBits(_xBtEventGroup, BT_SERVICE_STARTED_BIT | BT_SERVICE_START_FAILED_BIT, pdTRUE, pdFALSE, SERVICE_START_TIMEOUT / portTICK_PERIOD_MS);
            if ((bits & BT_SERVICE_STARTED_BIT) && !(bits & BT_SERVICE_START_FAILED_BIT))
            {
                request->StatusCode = StatusCodes::CompletedOk;
                RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::BluetoothStartedEvent, StatusCodes::CompletedOk);
            }
            else
            {
                if (bits & BT_SERVICE_START_FAILED_BIT)
                {
                    ERROR_MESSAGE("Start: the Bluetooth service did not start.");
                    request->StatusCode = StatusCodes::Failure;
                }
                else
                {
                    ERROR_MESSAGE("Start: no word from the Bluetooth stack after %" PRIu32 " ms.", SERVICE_START_TIMEOUT);
                    request->StatusCode = StatusCodes::Timeout;
                }
            }
        }
        else
        {
            request->StatusCode = StatusCodes::Failure;
        }
    }

    request->MessageType = MessageTypes::Response;

    _messageDispatcher->QueueMessageForStm32(request);

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

                if (param->reg.status != ESP_GATT_OK)
                {
                    ERROR_MESSAGE("Registering the GATT application failed, status 0x%" PRIx32, static_cast<uint32_t>(param->reg.status));
                    GetInstance()->SignalServiceStart(false);
                    break;
                }

                _gattsInterfaceHandle = gatts_if;
                ret = esp_ble_gap_set_device_name(_manager.GetDeviceName());
                if (ret)
                {
                    ERROR_MESSAGE("Set device name failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }

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
                    GetInstance()->SignalServiceStart(false);
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
                    bool handleFound = _manager.GetCurrentValueForAttributeHandle(param->read.handle, rsp.attr_value.value, static_cast<uint16_t>(sizeof(rsp.attr_value.value)), &rsp.attr_value.len);

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
                TRACE_MESSAGE("ESP_GATTS_WRITE_EVT, write handle 0x%04" PRIx16 ", offset %" PRIu16 ", length %" PRIu16, param->write.handle, param->write.offset, param->write.len);
                //
                //  Every write to a characteristic's value reaches the STM32, whether or not
                //  the client wants a response: a write without response is the usual way to
                //  send data quickly (issue #299).  A prepared write is one piece of a long
                //  value; the pieces are delivered together when the client executes them.
                //
                //  Descriptors, such as the client configuration a client writes to
                //  subscribe, are answered by Bluedroid (ESP_GATT_AUTO_RSP).  Their writes
                //  are reported here too, with need_rsp false, but were never passed to the
                //  STM32 and still are not.
                //
                esp_gatt_status_t status = ESP_GATT_OK;
                esp_gatt_rsp_t *response = nullptr;
                if (param->write.is_prep)
                {
                    status = PrepareWrite(param->write.handle, param->write.offset, param->write.value, param->write.len);
                    //
                    //  A prepared write is answered with the piece it carried, so the client
                    //  can check it arrived intact.
                    //
                    if (param->write.need_rsp)
                    {
                        response = static_cast<esp_gatt_rsp_t *>(pvPortMalloc(sizeof(esp_gatt_rsp_t)));
                        if (response == nullptr)
                        {
                            status = ESP_GATT_NO_RESOURCES;
                        }
                        else
                        {
                            memset(response, 0, sizeof(esp_gatt_rsp_t));
                            response->attr_value.handle = param->write.handle;
                            response->attr_value.offset = param->write.offset;
                            response->attr_value.len = param->write.len;
                            response->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
                            memcpy(response->attr_value.value, param->write.value, param->write.len);
                        }
                    }
                }
                else
                {
                    if (param->write.need_rsp || _manager.IsCharacteristicValueHandle(param->write.handle))
                    {
                        OnDataWriteRequest(param->write.handle, param->write.value, param->write.len);
                    }
                }

                if (param->write.need_rsp)
                {
                    ret = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, response);
                    if (ret)
                    {
                        ERROR_MESSAGE("esp_ble_gatts_send_response failed, error code = %" PRIi32, ret);
                    }
                    else
                    {
                        TRACE_MESSAGE("esp_ble_gatts_send_response succeeded");
                    }
                }
                Utils::DeleteAndNullifyPointer(response);
            }
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            TRACE_MESSAGE("ESP_GATTS_EXEC_WRITE_EVT, flag %" PRIu8, param->exec_write.exec_write_flag);
            ExecuteWrite(param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC);
            ret = esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, nullptr);
            if (ret)
            {
                ERROR_MESSAGE("esp_ble_gatts_send_response failed, error code = %" PRIi32, ret);
            }
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
            if (param->start.status == ESP_GATT_OK)
            {
                GetInstance()->SignalServiceStart(true);
            }
            else
            {
                ERROR_MESSAGE("Starting the service failed, status 0x%" PRIx32, static_cast<uint32_t>(param->start.status));
                GetInstance()->SignalServiceStart(false);
            }
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
                ret = esp_ble_gap_update_conn_params(&conn_params);
                if (ret)
                {
                    ERROR_MESSAGE("Update connection parameters failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }
            }
            GetInstance()->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientConnectedEvent, StatusCodes::CompletedOk);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            TRACE_MESSAGE("ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%" PRIx32, param->disconnect.reason);
            //
            //  A long write the client did not finish before it went is not delivered.
            //
            DiscardPreparedWrite();
            ResetClientConfigurations();
            /* start advertising again when missing the connect */
            ret = esp_ble_gap_start_advertising(GetInstance()->GetAdvertisingParameters());
            if (ret)
            {
                ERROR_MESSAGE("Start advertising failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
            }
            _connectionHandle = INVALID_CONNECTION_HANDLE;
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
            //
            //  Every failure below must reach Start(), which is waiting to answer the STM32.
            //  A table holding more than one primary service is refused here, for example.
            //
            if (param->add_attr_tab.status != ESP_GATT_OK)
            {
                ERROR_MESSAGE("Create attribute table failed, error code=0x%" PRIx32, param->add_attr_tab.status);
                GetInstance()->SignalServiceStart(false);
            }
            else
            {
                if (param->add_attr_tab.num_handle != GetInstance()->GetAttributeTableLength())
                {
                    ERROR_MESSAGE("Create attribute table failed, num_handle (%" PRIu16 ") does not match the expected attribute table length (%" PRIu16 ")", param->add_attr_tab.num_handle, GetInstance()->GetAttributeTableLength());
                    GetInstance()->SignalServiceStart(false);
                }
                else
                {
                    TRACE_MESSAGE("Create attribute table successfully, handle count = %" PRIu16, param->add_attr_tab.num_handle);
                    // the assigned handles with be in param->add_attr_tab.handles - copy them off
                    if (!_manager.SetHandles(param->add_attr_tab.handles, param->add_attr_tab.num_handle))
                    {
                        //
                        //  The num_handle check above should make this unreachable, but SetHandles
                        //  is not trusting that on its own (issue #350).  Without every handle
                        //  applied the service graph is not safe to start.
                        //
                        ERROR_MESSAGE("SetHandles failed, the service graph needed more than %" PRIu16 " handles", param->add_attr_tab.num_handle);
                        GetInstance()->SignalServiceStart(false);
                    }
                    else
                    {
                        // now fire up the service (which is the first attribute in the attribute table) by handle.
                        // TODO: right now we're cheating
                        TRACE_MESSAGE("Starting service with handle 0x%04" PRIx16, param->add_attr_tab.handles[0]);
                        GetInstance()->_serviceHandle = param->add_attr_tab.handles[0];
                        ret = esp_ble_gatts_start_service(param->add_attr_tab.handles[0]);
                        if (ret != ESP_OK)
                        {
                            ERROR_MESSAGE("Requesting the service start failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                            GetInstance()->SignalServiceStart(false);
                        }
                    }
                }
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
                esp_err_t ret = esp_ble_gap_start_advertising(GetInstance()->GetAdvertisingParameters());
                if (ret)
                {
                    ERROR_MESSAGE("Start advertising failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }
            }
            break;
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: // 0
            TRACE_MESSAGE("ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT");
            // triggered by the gatts_event_handler for ESP_GATTS_REG_EVT where we set the advertising data
            _advertisingDataConfigured = false;
            if (!_responseDataConfigured)
            {
                TRACE_MESSAGE("Advertising data set.  Starting advertising...");
                esp_err_t ret = esp_ble_gap_start_advertising(GetInstance()->GetAdvertisingParameters());
                if (ret)
                {
                    ERROR_MESSAGE("Start advertising failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }
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
            {
                TRACE_MESSAGE("ESP_GAP_BLE_SEC_REQ_EVT");
                // security request
                esp_err_t ret = esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
                if (ret)
                {
                    ERROR_MESSAGE("Security response failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }
            }
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