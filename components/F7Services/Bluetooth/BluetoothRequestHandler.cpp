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
BluetoothPairing BluetoothRequestHandler::_pairing;

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
    //
    //  The Bluetooth stack calls GAPEventHandler and GATTSProfileEventHandler, and reads the attribute table, for
    //  as long as it is running.  There is no way to unregister those callbacks, so the stack is taken down
    //  first, while there is still a handler for them to find, and only then is what they use released and the
    //  handler forgotten.  Nothing is up in the usual case, and this does nothing (issue #183).
    //
    TearDownStack();
    DiscardPreparedWrite();
    _pairing.Reset();

    ReleaseAttributeTable();

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
            //
            //  A declaration is read only and needs no security (Bluetooth Core Specification,
            //  Vol 3, Part G, 3.3.1).  It used to take the characteristic's permission, so a
            //  characteristic that needs encryption could not even be found by a client that had
            //  not yet paired, and the client never learnt it had to pair (issue #398).
            //
            _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, reinterpret_cast<uint8_t *>(&characteristic->GetDeclaration().Uuid), ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, reinterpret_cast<uint8_t *>(&characteristic->GetDeclaration().Properties)}};

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
    if (response.Handles != nullptr)
    {
        memset(response.Handles, 0, response.HandlesLength);
    }

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
 * @brief Release the attribute table and set its length back to 0.
 *
 *  The length is the number of handles GetHandles reports, so it has to go with the service
 *  graph the table was built from.  Left behind, GetHandles sent that many handles of
 *  uninitialised memory for a graph that no longer existed (issue #636).
 */
void BluetoothRequestHandler::ReleaseAttributeTable()
{
    delete[] _attribute_table;
    _attribute_table = nullptr;
    _attributeTableLength = 0;
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
        case BluetoothFunction::PasskeyReply:
            PasskeyReply(request);
            break;
        case BluetoothFunction::NumericComparisonReply:
            NumericComparisonReply(request);
            break;
        case BluetoothFunction::GetBondedDevices:
            GetBondedDevices(request);
            break;
        case BluetoothFunction::RemoveBond:
            RemoveBond(request);
            break;
        case BluetoothFunction::RemoveAllBonds:
            RemoveAllBonds(request);
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

    TRACE_MESSAGE("Start: ApplySecurityParameters");
    if (!ApplySecurityParameters(_manager.GetSecurityConfiguration()))
    {
        TearDownStack();
        return false;
    }

    TRACE_MESSAGE("StartBluetoothService: Exit");

    return true;
}

/**
 * @brief Give the stack the pairing and bonding settings.
 *
 *  This used to be the ESP-IDF example's settings: MITM asked for with no input or output to
 *  provide it, an unused static passkey, and any security accepted.  The stack now has exactly
 *  what the STM32 configured, and no static passkey, so every passkey is random (issue #398).
 *
 * @param security Settings to give the stack.
 *
 * @return true if the stack took every setting.
 */
bool BluetoothRequestHandler::ApplySecurityParameters(const BluetoothSecurityConfiguration &security)
{
    esp_ble_auth_req_t authenticationRequirement = security.AuthenticationRequirement();
    esp_ble_io_cap_t ioCapability = static_cast<esp_ble_io_cap_t>(security.Capability);
    uint8_t keySize = BluetoothSecurityConfiguration::KEY_SIZE;
    uint8_t onlyAcceptSpecified = security.OnlyAcceptSpecifiedAuthentication();
    uint8_t outOfBand = ESP_BLE_OOB_DISABLE;
    //
    //  This device is the peripheral.  The initiator keys are those the client is asked to send
    //  and the responder keys those this device sends: the long term key, and the identity key so
    //  that a client using a private address can be recognised when it reconnects.
    //
    uint8_t initiatorKeys = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t responderKeys = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    struct
    {
        esp_ble_sm_param_t Parameter;
        void *Value;
        const char *Name;
    } const parameters[] = {
        {ESP_BLE_SM_AUTHEN_REQ_MODE, &authenticationRequirement, "authentication requirement"},
        {ESP_BLE_SM_IOCAP_MODE, &ioCapability, "input and output capability"},
        {ESP_BLE_SM_MAX_KEY_SIZE, &keySize, "maximum key size"},
        {ESP_BLE_SM_MIN_KEY_SIZE, &keySize, "minimum key size"},
        {ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &onlyAcceptSpecified, "only accept the specified authentication"},
        {ESP_BLE_SM_OOB_SUPPORT, &outOfBand, "out of band support"},
        {ESP_BLE_SM_SET_INIT_KEY, &initiatorKeys, "initiator keys"},
        {ESP_BLE_SM_SET_RSP_KEY, &responderKeys, "responder keys"},
    };

    bool applied = true;
    for (const auto &parameter: parameters)
    {
        const esp_err_t result = esp_ble_gap_set_security_param(parameter.Parameter, parameter.Value, sizeof(uint8_t));
        if (result != ESP_OK)
        {
            ERROR_MESSAGE("ApplySecurityParameters: the stack refused the %s, error %" PRIi32 " (%s)", parameter.Name, static_cast<int32_t>(result), esp_err_to_name(result));
            applied = false;
        }
    }

    TRACE_MESSAGE(
        "ApplySecurityParameters: authentication 0x%02" PRIx8 ", capability %" PRIu8 ", only accept specified %" PRIu8 ", encrypt on connect %d, accept pairing %d", authenticationRequirement, static_cast<uint8_t>(ioCapability), onlyAcceptSpecified, security.EncryptOnConnect, security.AcceptPairing);

    return (applied);
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
 * @brief Tell RemoveBond whether the stack removed the bond.
 *
 *  Called from the GAP event handler, on the Bluetooth stack's task, while RemoveOneBond waits on
 *  the Bluetooth request handler's task.
 *
 * @param removed true if the bond was removed.
 */
void BluetoothRequestHandler::SignalBondRemoved(const bool removed)
{
    xEventGroupSetBits(_xBtEventGroup, removed ? BT_BOND_REMOVED_BIT : BT_BOND_REMOVE_FAILED_BIT);
}

/**
 * @brief Answer a PasskeyRequestEvent with the passkey the user entered.
 *
 * @param request Request from the STM32, a BTPairingReply.
 */
void BluetoothRequestHandler::PasskeyReply(Message * const request)
{
    ReplyToPairing(request, BluetoothPairing::PendingReply::Passkey);
}

/**
 * @brief Answer a NumericComparisonRequestEvent.
 *
 * @param request Request from the STM32, a BTPairingReply.
 */
void BluetoothRequestHandler::NumericComparisonReply(Message * const request)
{
    ReplyToPairing(request, BluetoothPairing::PendingReply::NumericComparison);
}

/**
 * @brief Pass the STM32's answer to a question about pairing to the stack.
 *
 *  The answer goes to the stack only if it is the one due, for the client the question was about.
 *  A reply nobody asked for, or for another client, is refused rather than allowed to confirm a
 *  pairing the user never saw.  A passkey of more than six digits is refused but the question
 *  stays open, so a corrected reply can follow before the pairing times out.
 *
 * @param request Request from the STM32, a BTPairingReply.
 * @param reply Kind of reply the request is.
 */
void BluetoothRequestHandler::ReplyToPairing(Message * const request, const BluetoothPairing::PendingReply reply)
{
    TRACE_MESSAGE("ReplyToPairing: Enter");

    const bool hasPayload = (request->Payload != nullptr);
    bool malformed = false;
    Esp32Messaging::BTPairingReply *answer = hasPayload ? Encoders::ExtractBTPairingReply(request->Payload, request->PayloadLength, &malformed) : nullptr;
    request->DeletePayload();

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (answer == nullptr)
    {
        ERROR_MESSAGE("ReplyToPairing: the reply is missing or malformed.");
        result = (hasPayload && !malformed) ? StatusCodes::EspOutOfMemory : StatusCodes::InvalidRequest;
    }
    else
    {
        const bool accept = (answer->Accept != 0);
        if ((reply == BluetoothPairing::PendingReply::Passkey) && accept && !BluetoothSecurityConfiguration::IsValidPasskey(answer->Passkey))
        {
            ERROR_MESSAGE("ReplyToPairing: passkey %" PRIu32 " has more than six digits.", answer->Passkey);
            result = StatusCodes::InvalidRequest;
        }
        else
        {
            if (!_pairing.Take(reply, answer->Address))
            {
                ERROR_MESSAGE("ReplyToPairing: no such question about this client is waiting for a reply.");
                result = StatusCodes::InvalidRequest;
            }
            else
            {
                esp_err_t sent = ESP_OK;
                if (reply == BluetoothPairing::PendingReply::Passkey)
                {
                    sent = esp_ble_passkey_reply(answer->Address, accept, answer->Passkey);
                }
                else
                {
                    sent = esp_ble_confirm_reply(answer->Address, accept);
                }
                if (sent != ESP_OK)
                {
                    ERROR_MESSAGE("ReplyToPairing: the stack refused the reply, error %" PRIi32 " (%s)", static_cast<int32_t>(sent), esp_err_to_name(sent));
                    result = (sent == ESP_ERR_INVALID_STATE) ? StatusCodes::BluetoothNotStarted : StatusCodes::Failure;
                }
            }
        }
        Utils::DeleteAndNullifyPointer(answer);
    }

    request->MessageType = MessageTypes::Response;
    request->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(request);

    TRACE_MESSAGE("ReplyToPairing: Exit");
}

/**
 * @brief Get the devices that are bonded with this one.
 *
 * @param devices Set to the bonded devices.
 *
 * @return true if the stack listed them, false if it could not.
 */
bool BluetoothRequestHandler::ListBondedDevices(std::vector<esp_ble_bond_dev_t> &devices)
{
    devices.clear();
    const int count = esp_ble_get_bond_device_num();
    if (count < 0)
    {
        return (false);
    }
    if (count == 0)
    {
        return (true);
    }
    devices.resize(count);
    int listed = count;
    if (esp_ble_get_bond_device_list(&listed, devices.data()) != ESP_OK)
    {
        devices.clear();
        return (false);
    }
    devices.resize(listed);
    return (true);
}

/**
 * @brief Is this device bonded?
 *
 * @param address Address to look for in the bond list.
 *
 * @return true if it is in the bond list.
 */
bool BluetoothRequestHandler::IsBonded(const esp_bd_addr_t address)
{
    std::vector<esp_ble_bond_dev_t> devices;
    if (!ListBondedDevices(devices))
    {
        return (false);
    }
    for (const esp_ble_bond_dev_t &device: devices)
    {
        if (memcmp(device.bd_addr, address, sizeof(esp_bd_addr_t)) == 0)
        {
            return (true);
        }
    }
    return (false);
}

/**
 * @brief Send the STM32 the addresses of the bonded devices.
 *
 *  The bonds are kept by the stack, in flash, so the stack has to be running to list them.
 *
 * @param request Request from the STM32.
 */
void BluetoothRequestHandler::GetBondedDevices(Message * const request)
{
    TRACE_MESSAGE("GetBondedDevices: Enter");

    request->DeletePayload();
    std::vector<esp_ble_bond_dev_t> devices;
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
    {
        request->StatusCode = StatusCodes::BluetoothNotStarted;
    }
    else
    {
        if (!ListBondedDevices(devices))
        {
            ERROR_MESSAGE("GetBondedDevices: the stack could not list the bonded devices.");
            request->StatusCode = StatusCodes::Failure;
        }
        else
        {
            std::vector<uint8_t> entries;
            for (const esp_ble_bond_dev_t &device: devices)
            {
                entries.insert(entries.end(), device.bd_addr, device.bd_addr + sizeof(esp_bd_addr_t));
                entries.push_back(static_cast<uint8_t>(device.bd_addr_type));
            }
            Esp32Messaging::BTBondedDevicesResponse response;
            response.DeviceCount = static_cast<uint16_t>(devices.size());
            response.DevicesLength = static_cast<uint32_t>(entries.size());
            response.Devices = entries.data();

            request->PayloadLength = Encoders::EncodedBTBondedDevicesResponseBufferSize(&response);
            request->Payload = static_cast<uint8_t *>(pvPortMalloc(request->PayloadLength));
            if (request->Payload == nullptr)
            {
                request->PayloadLength = 0;
                request->StatusCode = StatusCodes::EspOutOfMemory;
            }
            else
            {
                Encoders::EncodeBTBondedDevicesResponse(&response, request->Payload);
                request->StatusCode = StatusCodes::CompletedOk;
            }
        }
    }

    request->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(request);

    TRACE_MESSAGE("GetBondedDevices: Exit");
}

/**
 * @brief Ask the stack to remove one bond and wait for it to finish.
 *
 *  A client that is connected when its bond is removed is disconnected by the stack.
 *
 * @param address Device whose bond is to be removed.
 *
 * @return CompletedOk, BluetoothDeviceNotBonded, Timeout or Failure.
 */
StatusCodes::StatusCodes BluetoothRequestHandler::RemoveOneBond(const esp_bd_addr_t address)
{
    if (!IsBonded(address))
    {
        return (StatusCodes::BluetoothDeviceNotBonded);
    }

    esp_bd_addr_t device;
    memcpy(device, address, sizeof(device));
    xEventGroupClearBits(_xBtEventGroup, BT_BOND_REMOVED_BIT | BT_BOND_REMOVE_FAILED_BIT);
    const esp_err_t requested = esp_ble_remove_bond_device(device);
    if (requested != ESP_OK)
    {
        ERROR_MESSAGE("RemoveOneBond: the stack refused, error %" PRIi32 " (%s)", static_cast<int32_t>(requested), esp_err_to_name(requested));
        return (StatusCodes::Failure);
    }

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    const EventBits_t bits = xEventGroupWaitBits(_xBtEventGroup, BT_BOND_REMOVED_BIT | BT_BOND_REMOVE_FAILED_BIT, pdTRUE, pdFALSE, BOND_REMOVE_TIMEOUT / portTICK_PERIOD_MS);
    if ((bits & BT_BOND_REMOVED_BIT) && !(bits & BT_BOND_REMOVE_FAILED_BIT))
    {
        _pairing.ForgetClient(address);
    }
    else
    {
        if (bits & BT_BOND_REMOVE_FAILED_BIT)
        {
            ERROR_MESSAGE("RemoveOneBond: the stack could not remove the bond.");
            result = StatusCodes::Failure;
        }
        else
        {
            ERROR_MESSAGE("RemoveOneBond: no word from the stack after %" PRIu32 " ms.", BOND_REMOVE_TIMEOUT);
            result = StatusCodes::Timeout;
        }
    }
    return (result);
}

/**
 * @brief Remove the bond with one device.
 *
 * @param request Request from the STM32, a BTDeviceAddress.
 */
void BluetoothRequestHandler::RemoveBond(Message * const request)
{
    TRACE_MESSAGE("RemoveBond: Enter");

    const bool hasPayload = (request->Payload != nullptr);
    bool malformed = false;
    Esp32Messaging::BTDeviceAddress *device = hasPayload ? Encoders::ExtractBTDeviceAddress(request->Payload, request->PayloadLength, &malformed) : nullptr;
    request->DeletePayload();

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (device == nullptr)
    {
        ERROR_MESSAGE("RemoveBond: the address is missing or malformed.");
        result = (hasPayload && !malformed) ? StatusCodes::EspOutOfMemory : StatusCodes::InvalidRequest;
    }
    else
    {
        if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
        {
            result = StatusCodes::BluetoothNotStarted;
        }
        else
        {
            result = RemoveOneBond(device->Address);
        }
        Utils::DeleteAndNullifyPointer(device);
    }

    request->MessageType = MessageTypes::Response;
    request->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(request);

    TRACE_MESSAGE("RemoveBond: Exit");
}

/**
 * @brief Remove the bond with every device.
 *
 *  Every bond is tried even after one fails, and the first failure is reported.
 *
 * @param request Request from the STM32.
 */
void BluetoothRequestHandler::RemoveAllBonds(Message * const request)
{
    TRACE_MESSAGE("RemoveAllBonds: Enter");

    request->DeletePayload();
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    std::vector<esp_ble_bond_dev_t> devices;
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
    {
        result = StatusCodes::BluetoothNotStarted;
    }
    else
    {
        if (!ListBondedDevices(devices))
        {
            ERROR_MESSAGE("RemoveAllBonds: the stack could not list the bonded devices.");
            result = StatusCodes::Failure;
        }
        else
        {
            for (const esp_ble_bond_dev_t &device: devices)
            {
                const StatusCodes::StatusCodes removed = RemoveOneBond(device.bd_addr);
                if ((removed != StatusCodes::CompletedOk) && (result == StatusCodes::CompletedOk))
                {
                    result = removed;
                }
            }
        }
    }

    request->MessageType = MessageTypes::Response;
    request->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(request);

    TRACE_MESSAGE("RemoveAllBonds: Exit");
}

/**
 * @brief Answer a client's request to pair, as the security settings allow.
 *
 *  This used to accept every request (issue #398).  How the pairing is then authenticated, and
 *  whether the result is good enough, is up to the stack, which has the security settings.
 *
 * @param address Client asking to pair.
 */
void BluetoothRequestHandler::OnSecurityRequest(esp_bd_addr_t address)
{
    const bool accept = _manager.GetSecurityConfiguration().AcceptPairing;
    if (!accept)
    {
        ERROR_MESSAGE("A client asked to pair; pairing is not accepted, refusing it.");
    }
    const esp_err_t result = esp_ble_gap_security_rsp(address, accept);
    if (result != ESP_OK)
    {
        ERROR_MESSAGE("Security response failed, error code = %" PRIi32 ", %s", static_cast<int32_t>(result), esp_err_to_name(result));
    }
}

/**
 * @brief Tell the STM32 about a step in pairing that needs the user.
 *
 * @param event PasskeyDisplayEvent, PasskeyRequestEvent or NumericComparisonRequestEvent.
 * @param address Client being paired with.
 * @param passkey Number to show the user, 0 when one is being asked for.
 */
void BluetoothRequestHandler::RaisePairingEvent(const BluetoothFunction::BluetoothFunction event, const esp_bd_addr_t address, const uint32_t passkey)
{
    BluetoothRequestHandler * const instance = GetInstance();
    if (instance == nullptr)
    {
        return;
    }

    Esp32Messaging::BTPairingEventData data;
    memcpy(data.Address, address, sizeof(data.Address));
    data.Passkey = passkey;
    const int payloadLength = Encoders::EncodedBTPairingEventDataBufferSize(&data);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    if (payload == nullptr)
    {
        ERROR_MESSAGE("RaisePairingEvent: Failed to allocate memory for the event payload.");
        return;
    }
    Encoders::EncodeBTPairingEventData(&data, payload);
    instance->RaiseEvent(Esp32Interfaces::BlueTooth, event, StatusCodes::CompletedOk, payload, payloadLength);
}

/**
 * @brief Act on the end of pairing, or of encryption with a bonded client, and tell the STM32.
 *
 *  A bonded client that reconnects encrypts with its stored keys, which ends here too.  It is
 *  given back the subscriptions it had when it last disconnected.
 *
 * @param complete What the stack reported.
 */
void BluetoothRequestHandler::OnAuthenticationComplete(const esp_ble_auth_cmpl_t &complete)
{
    _pairing.ClearPendingReply();

    Esp32Messaging::BTAuthenticationCompleteEventData data;
    memcpy(data.Address, complete.bd_addr, sizeof(data.Address));
    data.AddressType = static_cast<uint8_t>(complete.addr_type);
    data.AuthenticationMode = static_cast<uint8_t>(complete.auth_mode);
    data.FailureReason = complete.success ? 0 : static_cast<uint8_t>(complete.fail_reason);
    data.Bonded = (complete.success && IsBonded(complete.bd_addr)) ? 1 : 0;

    if (complete.success)
    {
        TRACE_MESSAGE("Authentication succeeded, mode 0x%02" PRIx8 ", bonded %" PRIu8, data.AuthenticationMode, data.Bonded);
    }
    else
    {
        ERROR_MESSAGE("Authentication failed, reason 0x%02" PRIx8, data.FailureReason);
    }
    if (data.Bonded != 0)
    {
        _pairing.SetBondedClient(complete.bd_addr);
        RestoreClientConfigurations(complete.bd_addr);
    }

    BluetoothRequestHandler * const instance = GetInstance();
    if (instance == nullptr)
    {
        return;
    }
    const int payloadLength = Encoders::EncodedBTAuthenticationCompleteEventDataBufferSize(&data);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    if (payload == nullptr)
    {
        ERROR_MESSAGE("OnAuthenticationComplete: Failed to allocate memory for the event payload.");
        return;
    }
    Encoders::EncodeBTAuthenticationCompleteEventData(&data, payload);
    instance->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::AuthenticationCompleteEvent, complete.success ? StatusCodes::CompletedOk : StatusCodes::Failure, payload, payloadLength);
}

/**
 * @brief Act on a client connecting.
 *
 *  Nothing from an earlier connection carries over until the client has proved, by encrypting
 *  with its keys, that it is the bonded client it claims to be.
 *
 * @param address Client that has connected.
 */
void BluetoothRequestHandler::OnClientConnected(esp_bd_addr_t address)
{
    _pairing.ClearBondedClient();
    _pairing.ClearPendingReply();

    const BluetoothSecurityConfiguration security = _manager.GetSecurityConfiguration();
    if (security.EncryptOnConnect)
    {
        const esp_err_t result = esp_ble_set_encryption(address, security.EncryptionLevel());
        if (result != ESP_OK)
        {
            ERROR_MESSAGE("Asking for encryption failed, error code = %" PRIi32 ", %s", static_cast<int32_t>(result), esp_err_to_name(result));
        }
    }
}

/**
 * @brief Act on the client disconnecting: keep its subscriptions if it is bonded, then clear them.
 */
void BluetoothRequestHandler::OnClientDisconnected()
{
    //
    //  Removing a connected client's bond disconnects it, and the disconnection can arrive after
    //  RemoveOneBond has forgotten the client.  So the bond list is checked again rather than
    //  keeping subscriptions for a client that is no longer bonded.
    //
    esp_bd_addr_t bondedClient;
    if (_pairing.GetBondedClient(bondedClient) && IsBonded(bondedClient))
    {
        _pairing.SaveClientConfigurations(bondedClient, ReadClientConfigurations());
    }
    _pairing.ClearBondedClient();
    _pairing.ClearPendingReply();
    ResetClientConfigurations();
}

/**
 * @brief Read the value of every Client Characteristic Configuration descriptor from the stack.
 *
 * @return The values, in the order of the descriptors in the service graph.
 */
std::vector<BluetoothPairing::ClientConfiguration> BluetoothRequestHandler::ReadClientConfigurations()
{
    std::vector<BluetoothPairing::ClientConfiguration> configurations;
    for (const uint16_t handle: _manager.GetClientConfigurationHandles())
    {
        uint16_t length = 0;
        const uint8_t *value = nullptr;
        if ((esp_ble_gatts_get_attr_value(handle, &length, &value) == ESP_GATT_OK) && (value != nullptr) && (length >= CLIENT_CHARACTERISTIC_CONFIGURATION_LENGTH))
        {
            configurations.push_back({handle, static_cast<uint16_t>(value[0] | (value[1] << 8))});
        }
    }
    return (configurations);
}

/**
 * @brief Give a bonded client back the subscriptions it had when it last disconnected.
 *
 * @param address Address the client is bonded with.
 */
void BluetoothRequestHandler::RestoreClientConfigurations(const esp_bd_addr_t address)
{
    for (const BluetoothPairing::ClientConfiguration &configuration: _pairing.GetClientConfigurations(address))
    {
        const uint8_t value[CLIENT_CHARACTERISTIC_CONFIGURATION_LENGTH] = {static_cast<uint8_t>(configuration.Value & 0x00ff), static_cast<uint8_t>((configuration.Value & 0xff00) >> 8)};
        if (esp_ble_gatts_set_attr_value(configuration.Handle, sizeof(value), value) != ESP_OK)
        {
            ERROR_MESSAGE("Could not restore the client configuration at handle 0x%04" PRIx16, configuration.Handle);
        }
    }
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

    const bool stopped = StopBluetoothService();
    //
    //  The bonds themselves are kept by the stack, in flash.  The subscriptions kept for bonded
    //  clients name attribute handles that the next configuration may give out differently.
    //
    _pairing.Reset();
    if (stopped)
    {
        ReleaseAttributeTable();
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

    //
    //  The write comes from the Bluetooth stack, which can outlive the handler, so there may be nobody to tell (issue #183).
    //
    BluetoothRequestHandler * const instance = GetInstance();
    if (instance == nullptr)
    {
        ERROR_MESSAGE("OnDataWriteRequest: There is no Bluetooth handler, ignoring the write.");
        return;
    }

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

    instance->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientWriteRequestEvent, StatusCodes::CompletedOk, payload, payload_length);

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
        if (!graphBuilt)
        {
            ReleaseAttributeTable();
        }
    }

    if (!graphBuilt)
    {
        request->StatusCode = refusal;
    }
    else
    {
        GenerateAdvertisingData();
        GenerateAttributeTable();
        _pairing.Reset();
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

    //
    //  The stack calls this for as long as it is running, and does not know when the handler has gone.  An event
    //  that arrives with no handler has nobody to be reported to and nothing to act on, so it is ignored rather
    //  than dereferencing a null pointer (issue #183).
    //
    BluetoothRequestHandler * const instance = GetInstance();
    if (instance == nullptr)
    {
        ERROR_MESSAGE("GATTSProfileEventHandler: Event %d arrived with no Bluetooth handler, ignoring it.", static_cast<int>(event));
        return;
    }

    esp_err_t ret = ESP_OK;
    switch (event)
    {
        case ESP_GATTS_REG_EVT:
            {
                TRACE_MESSAGE("ESP_GATTS_REG_EVT");

                if (param->reg.status != ESP_GATT_OK)
                {
                    ERROR_MESSAGE("Registering the GATT application failed, status 0x%" PRIx32, static_cast<uint32_t>(param->reg.status));
                    instance->SignalServiceStart(false);
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

                ret = esp_ble_gap_config_adv_data(instance->GetAdvertisingData());
                if (ret)
                {
                    ERROR_MESSAGE("Config adv data failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }
                _advertisingDataConfigured = true;

                // configure our BLE scan response
                ret = esp_ble_gap_config_adv_data(instance->GetScanResponseData());
                if (ret)
                {
                    ERROR_MESSAGE("Config scan response data failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                }
                _responseDataConfigured = true;

                TRACE_MESSAGE("BLE advertising configured.  Creating attributes...");

                // set the attribute table - this requires the services be already-defined in the Manager
                esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(instance->GetAttributeTable(), gatts_if, instance->GetAttributeTableLength(), BluetoothRequestHandler::INSTANCE_ID);
                if (create_attr_ret)
                {
                    ERROR_MESSAGE("Create attr table failed, error code = %" PRIi32, create_attr_ret);
                    instance->SignalServiceStart(false);
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

                    //
                    //  A value longer than the MTU is read in pieces: a Read Request, then Read Blob
                    //  Requests at increasing offsets.  Each gets the part of the value from its
                    //  offset (issue #528).
                    //
                    rsp.attr_value.offset = param->read.offset;

                    // go get the currently known value from the manager
                    const esp_gatt_status_t status = _manager.GetCurrentValueForAttributeHandle(param->read.handle, param->read.offset, rsp.attr_value.value, static_cast<uint16_t>(sizeof(rsp.attr_value.value)), &rsp.attr_value.len);

                    TRACE_MESSAGE("  Sending read response of %" PRIu16 " bytes from offset %" PRIu16, rsp.attr_value.len, param->read.offset);
                    ret = esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, status, &rsp);

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
            // if (esp_ble_gatts_stop_service(instance->_serviceHandle) != ESP_OK)
            // {
            //     ERROR_MESSAGE("Failed to stop service");
            // }
            // instance->_serviceHandle = -1;
            break;
        case ESP_GATTS_DELETE_EVT:
            TRACE_MESSAGE("ESP_GATTS_DELETE_EVT");

            break;
        case ESP_GATTS_START_EVT:
            TRACE_MESSAGE("ESP_GATTS_START_EVT");
            if (param->start.status == ESP_GATT_OK)
            {
                instance->SignalServiceStart(true);
            }
            else
            {
                ERROR_MESSAGE("Starting the service failed, status 0x%" PRIx32, static_cast<uint32_t>(param->start.status));
                instance->SignalServiceStart(false);
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
                TRACE_MESSAGE("    Connection ID 0x%04" PRIx16, param->connect.conn_id);
                _connectionHandle = param->connect.conn_id;
                OnClientConnected(param->connect.remote_bda);
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
            instance->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientConnectedEvent, StatusCodes::CompletedOk);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            TRACE_MESSAGE("ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%" PRIx32, param->disconnect.reason);
            //
            //  A long write the client did not finish before it went is not delivered.
            //
            DiscardPreparedWrite();
            OnClientDisconnected();
            /* start advertising again when missing the connect */
            ret = esp_ble_gap_start_advertising(instance->GetAdvertisingParameters());
            if (ret)
            {
                ERROR_MESSAGE("Start advertising failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
            }
            _connectionHandle = INVALID_CONNECTION_HANDLE;
            instance->RaiseEvent(Esp32Interfaces::BlueTooth, BluetoothFunction::ClientDisconnectedEvent, StatusCodes::CompletedOk);
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
                instance->SignalServiceStart(false);
            }
            else
            {
                if (param->add_attr_tab.num_handle != instance->GetAttributeTableLength())
                {
                    ERROR_MESSAGE("Create attribute table failed, num_handle (%" PRIu16 ") does not match the expected attribute table length (%" PRIu16 ")", param->add_attr_tab.num_handle, instance->GetAttributeTableLength());
                    instance->SignalServiceStart(false);
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
                        instance->SignalServiceStart(false);
                    }
                    else
                    {
                        // now fire up the service (which is the first attribute in the attribute table) by handle.
                        // TODO: right now we're cheating
                        TRACE_MESSAGE("Starting service with handle 0x%04" PRIx16, param->add_attr_tab.handles[0]);
                        instance->_serviceHandle = param->add_attr_tab.handles[0];
                        ret = esp_ble_gatts_start_service(param->add_attr_tab.handles[0]);
                        if (ret != ESP_OK)
                        {
                            ERROR_MESSAGE("Requesting the service start failed, error code = %" PRIi32 ", %s", ret, esp_err_to_name(ret));
                            instance->SignalServiceStart(false);
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

    //
    //  See GATTSProfileEventHandler: an event with no handler to act on is ignored (issue #183).
    //
    BluetoothRequestHandler * const instance = GetInstance();
    if (instance == nullptr)
    {
        ERROR_MESSAGE("GAPEventHandler: Event %d arrived with no Bluetooth handler, ignoring it.", static_cast<int>(event));
        return;
    }

    switch (event)
    {
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: // 1
            TRACE_MESSAGE("ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT");
            _responseDataConfigured = false;
            if (!_advertisingDataConfigured)
            {
                TRACE_MESSAGE("Scan data set.  Starting advertising...");
                esp_err_t ret = esp_ble_gap_start_advertising(instance->GetAdvertisingParameters());
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
                esp_err_t ret = esp_ble_gap_start_advertising(instance->GetAdvertisingParameters());
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
            //
            //  The client is showing a passkey for the user to enter here.  The STM32 asks the
            //  user and answers with PasskeyReply.
            //
            TRACE_MESSAGE("ESP_GAP_BLE_PASSKEY_REQ_EVT");
            _pairing.Expect(BluetoothPairing::PendingReply::Passkey, param->ble_security.ble_req.bd_addr);
            RaisePairingEvent(BluetoothFunction::PasskeyRequestEvent, param->ble_security.ble_req.bd_addr, 0);
            break;
        case ESP_GAP_BLE_OOB_REQ_EVT: // 13
            //
            //  Out of band pairing is switched off (ESP_BLE_SM_OOB_SUPPORT), so this should not
            //  arrive.  It used to be answered with a fixed key anyone could know (issue #398);
            //  unanswered, the pairing fails when the stack's timer runs out.
            //
            ERROR_MESSAGE("ESP_GAP_BLE_OOB_REQ_EVT: out of band pairing is not supported, not answering.");
            break;
        case ESP_GAP_BLE_LOCAL_IR_EVT: /* BLE local IR event */
            TRACE_MESSAGE("ESP_GAP_BLE_LOCAL_IR_EVT");
            break;
        case ESP_GAP_BLE_LOCAL_ER_EVT: /* BLE local ER event */
            TRACE_MESSAGE("ESP_GAP_BLE_LOCAL_ER_EVT");
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:   // 16
            //
            //  Numeric comparison: both sides show the same six digits and the user says whether
            //  they match.  This used to be confirmed without asking anyone, which gave none of
            //  the protection it exists for (issue #398).  The STM32 asks the user and answers
            //  with NumericComparisonReply.
            //
            TRACE_MESSAGE("ESP_GAP_BLE_NC_REQ_EVT, number %06" PRIu32, param->ble_security.key_notif.passkey);
            _pairing.Expect(BluetoothPairing::PendingReply::NumericComparison, param->ble_security.key_notif.bd_addr);
            RaisePairingEvent(BluetoothFunction::NumericComparisonRequestEvent, param->ble_security.key_notif.bd_addr, param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT: // 10
            TRACE_MESSAGE("ESP_GAP_BLE_SEC_REQ_EVT");
            OnSecurityRequest(param->ble_security.ble_req.bd_addr);
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: // 11
            //
            //  The user types this passkey into the client; the STM32 shows it to them.
            //
            TRACE_MESSAGE("ESP_GAP_BLE_PASSKEY_NOTIF_EVT, passkey %06" PRIu32, param->ble_security.key_notif.passkey);
            RaisePairingEvent(BluetoothFunction::PasskeyDisplayEvent, param->ble_security.key_notif.bd_addr, param->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_KEY_EVT:       // 9
            TRACE_MESSAGE("ESP_GAP_BLE_KEY_EVT key type = %" PRId32, param->ble_security.ble_key.key_type);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT: // 8
            TRACE_MESSAGE("ESP_GAP_BLE_AUTH_CMPL_EVT");
            OnAuthenticationComplete(param->ble_security.auth_cmpl);
            break;
        case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT: // 23
            TRACE_MESSAGE("ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT status = %" PRId32, param->remove_bond_dev_cmpl.status);
            TRACE_HEX_BUFFER(param->remove_bond_dev_cmpl.bd_addr, sizeof(esp_bd_addr_t));
            instance->SignalBondRemoved(param->remove_bond_dev_cmpl.status == ESP_BT_STATUS_SUCCESS);
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
                esp_err_t ret = esp_ble_gap_config_adv_data(instance->GetAdvertisingData());
                if (ret)
                {
                    ERROR_MESSAGE("Config adv data failed, error code = %" PRIi32, ret);
                }
                else
                {
                    _advertisingDataConfigured = true;
                }

                ret = esp_ble_gap_config_adv_data(instance->GetScanResponseData());
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
