/**
 * @file BluetoothManager.cpp
 * @author Chris Tacke
 *
 * @brief Implementation of the methods in the BluetoothManager class.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#include "Logging.hpp"

#include "BluetoothRequestHandler.hpp"

#include "BluetoothManager.hpp"

#include "../rapidjson/document.h"
#include "esp_log.h"

using namespace rapidjson;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Local preprocessor definitions.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Size of a character variable.
 */
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Name of the component (used for logging).
 */
const char *BluetoothManager::COMPONENT_NAME = BluetoothRequestHandler::COMPONENT_NAME;

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
 * @brief Construct a new Bluetooth Manager:: Bluetooth Manager object
 */
BluetoothManager::BluetoothManager() : Services(&this->_handleManager)
{
    TRACE_MESSAGE("BluetoothManager: Enter");

    Services.OnServiceAdded = &BluetoothManager::HandleServiceAdded;
    Services.OnCharacteristicAdded = &BluetoothManager::HandleCharacteristicAdded;
    Services.OnValueChanged = &BluetoothManager::HandleValueChanged;

    TRACE_MESSAGE("BluetoothManager: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Process the addition of a new service.
 *
 * @param service Service being added.
 */
void BluetoothManager::HandleServiceAdded(BluetoothService *service)
{
    TRACE_MESSAGE("HandleServiceAdded: Enter");

    TRACE_MESSAGE("New Service: 0x%04X (%s)", service->Handle, service->Name.c_str());

    TRACE_MESSAGE("HandleServiceAdded: Exit");
}

/**
 * @brief Process the addition of a new characteristic.
 *
 * @param characteristic Characteristic being added.
 */
void BluetoothManager::HandleCharacteristicAdded(BluetoothCharacteristic *characteristic)
{
    TRACE_MESSAGE("HandleCharacteristicAdded: Enter");

    TRACE_MESSAGE("New Characteristic: declaration @ 0x%04X value @ 0x%04X", characteristic->Declaration->Handle, characteristic->Value->Handle);

    TRACE_MESSAGE("HandleCharacteristicAdded: Exit");
}

/**
 * @brief Process the characteristic value changed event.
 *
 * @param characteristic Characteristic whose value is being changed.
 */
void BluetoothManager::HandleValueChanged(BluetoothCharacteristic *characteristic)
{
    TRACE_MESSAGE("HandleValueChanged: Enter");

    TRACE_MESSAGE("New Characteristic Value: value @ 0x%04X is %i bytes", characteristic->Value->Handle, characteristic->Value->CurrentLength);

    TRACE_MESSAGE("HandleValueChanged: Exit");
}

/**
 * @brief Handle the addition of a new descriptor.
 */
void BluetoothManager::HandleDescriptorAdded(BluetoothDescriptor *)
{
    TRACE_MESSAGE("HandleDescriptorAdded: Enter");

    TRACE_MESSAGE("HandleDescriptorAdded: Exit");
}

/**
 * @brief Get all of the handles associated with the BluetoothManager.
 *
 * @note It is assumed that the caller has allocated enough memory to hold the handles.
 *
 * @param handles Pointer to an array of uint16_t values to hold the handles.
 */
void BluetoothManager::GetHandles(uint16_t *handles)
{
    TRACE_MESSAGE("GetHandles: Enter");

    int index = 0;

    for (const auto &svc: Services.GetServiceDefinitions())
    {
        handles[index++] = svc->Handle;

        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            handles[index++] = characteristic->Declaration->Handle;
            handles[index++] = characteristic->Value->Handle;

            for (const auto &descriptor: characteristic->GetDescriptorDefinitions())
            {
                handles[index++] = descriptor->Handle;
            }
        }
    }

    TRACE_MESSAGE("GetHandles: Exit");
}

/**
 * @brief Set the handles for the services, characteristics and descriptors.
 *
 * @param handles Pointer to a block of memory holding the handles for the services, characteristics and descriptors.
 * @param count Number of handles.
 */
void BluetoothManager::SetHandles(uint16_t *handles, uint16_t count)
{
    TRACE_MESSAGE("SetHandles: Enter");

    TRACE_MESSAGE("Handles: ");
    for (int i = 0; i < count; i++)
    {
        TRACE_MESSAGE("   0x%04x ", handles[i]);
    }

    int index = 0;

    for (const auto &svc: Services.GetServiceDefinitions())
    {
        svc->Handle = handles[index++];
        TRACE_MESSAGE("ATTRIBUTE TABLE SERVICE HANDLE = 0x%04x", svc->Handle);

        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            characteristic->Declaration->Handle = handles[index++];
            TRACE_MESSAGE("ATTRIBUTE TABLE DECLARATION HANDLE = 0x%04x", characteristic->Declaration->Handle);

            characteristic->Value->Handle = handles[index++];
            TRACE_MESSAGE("ATTRIBUTE TABLE VALUE HANDLE = 0x%04x | max length = %i", characteristic->Value->Handle, characteristic->Value->MaxLength);
            auto val = length_val_s {.length = characteristic->Value->MaxLength, .value = new uint8_t[characteristic->Value->MaxLength]};
            memset(val.value, 0, characteristic->Value->MaxLength);

            TRACE_MESSAGE("Request for data for handle 0x%04x", characteristic->Value->Handle);

            _valueMap.insert({characteristic->Value->Handle, val});
            // cross reference handles
            characteristic->Declaration->ValueHandle = characteristic->Value->Handle;

            for (const auto &descriptor: characteristic->GetDescriptorDefinitions())
            {
                descriptor->Handle = handles[index++];
                TRACE_MESSAGE("ATTRIBUTE TABLE DESCRIPTOR HANDLE = 0x%04x", descriptor->Handle);
            }
        }
    }

    TRACE_MESSAGE("SetHandles: Exit");
}

/**
 * @brief Generate the advertising data for the device.
 */
void BluetoothManager::GenerateAdvertisingData()
{
    TRACE_MESSAGE("GenerateAdvertisingData: Enter");

    uint8_t *manufacturer_data = static_cast<uint8_t *>(pvPortMalloc(2));
    manufacturer_data[0] = (uint8_t) (_companyIdentifier & 0x00FF);
    manufacturer_data[1] = (uint8_t) ((_companyIdentifier & 0xFF00) >> 8);

    // NOTE: unknown if this specific value has meaning.  Pulled directly from espressif.
    //
    //  Move this to file/class scope.
    //
    // clang-format off
    uint8_t dummy_advertising_service_uuid[32] = {
        /* LSB <--------------------------------------------------------------------------------> MSB */
        // First UUID, 16bit, [12],[13] is the value
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xee, 0x00, 0x00, 0x00,
        // Second UUID, 32bit, [12], [13], [14], [15] is the value
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xee, 0x00, 0x00, 0x00,
    };
    // clang-format on
    uint8_t *advertising_service_uuid = static_cast<uint8_t *>(pvPortMalloc(sizeof(dummy_advertising_service_uuid)));
    memcpy(advertising_service_uuid, dummy_advertising_service_uuid, sizeof(dummy_advertising_service_uuid));

    memset(&_advertisingData, 0, sizeof(esp_ble_adv_data_t));
    /* The length of adv data must be less than 31 bytes */
    _advertisingData.set_scan_rsp = false;
    _advertisingData.include_name = true;
    _advertisingData.include_txpower = true;
    _advertisingData.min_interval = 0x0006; // slave connection min interval, Time = min_interval * 1.25 msec
    _advertisingData.max_interval = 0x0010; // slave connection max interval, Time = max_interval * 1.25 msec
    _advertisingData.appearance = 0x00;
    _advertisingData.manufacturer_len = 2;
    _advertisingData.p_manufacturer_data = manufacturer_data;
    _advertisingData.service_data_len = 0;
    _advertisingData.p_service_data = NULL;
    _advertisingData.service_uuid_len = sizeof(dummy_advertising_service_uuid);
    _advertisingData.p_service_uuid = advertising_service_uuid;
    _advertisingData.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    // Scan response data
    memset(&_scanResponseData, 0, sizeof(esp_ble_adv_data_t));
    _scanResponseData.set_scan_rsp = true;
    _scanResponseData.include_name = true;
    _scanResponseData.include_txpower = true;
    _scanResponseData.manufacturer_len = 2;
    _scanResponseData.p_manufacturer_data = manufacturer_data;

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
esp_gatts_attr_db_t *BluetoothManager::GetAttributeTable()
{
    return _attribute_table;
}

/**
 * @brief Get a pointer to the advertising data.
 *
 * @return esp_ble_adv_data_t* Pointer to the advertising data.
 */
esp_ble_adv_data_t *BluetoothManager::GetAdvertisingData()
{
    return &_advertisingData;
}

/**
 * @brief Get a pointer to the advertising parameters.
 *
 * @return esp_ble_adv_params_t* Pointer to the advertising parameters.
 */
esp_ble_adv_params_t *BluetoothManager::GetAdvertisingParameters()
{
    return &_advertisingParameters;
}

/**
 * @brief Get a pointer to the scan response data.
 *
 * @return esp_ble_adv_data_t* Pointer the scan response data.
 */
esp_ble_adv_data_t *BluetoothManager::GetScanResponseData()
{
    return &_scanResponseData;
}

/**
 * @brief Get the length of the attributes table.
 *
 * @return uint16_t Length of the attributes table.
 */
uint16_t BluetoothManager::GetAttributeTableLength()
{
    return _attributeTableLength;
}

/**
 * @brief Dummy variable.
 */
static int __attribute__((unused)) test_value = 0;

/**
 * @brief Get the current value for a given attribute handle.
 *
 * @param handle Handle to look up.
 * @param pData Pointer to a block of memory to take the value.
 * @param pLen Pointer to a variable that will hold the length of the data that is being copied.
 */
void BluetoothManager::GetCurrentValueForAttributeHandle(uint16_t handle, uint8_t *pData, uint16_t *pLen)
{
    TRACE_MESSAGE("GetCurrentValueForAttributeHandle: Enter");

    // TODO: go get the data from the STM - for now we just increment to test the data is changing
    auto existing = _valueMap.find(handle);

    if (existing != _valueMap.end())
    {
        TRACE_MESSAGE("Request for data for handle 0x%04x", handle);
        TRACE_MESSAGE(" known length = %i", existing->second.length);
        TRACE_HEX_BUFFER(existing->second.value, existing->second.length);

        memcpy(pData, existing->second.value, existing->second.length);
        *pLen = existing->second.length;
    }
    else
    {
        TRACE_MESSAGE("Request for data with unknown handle 0x%04x", handle);
        *pLen = 0;
        pData = NULL;
    }

    TRACE_MESSAGE("GetCurrentValueForAttributeHandle: Exit");
}

/**
 * @brief Set the value for the specified attribute handle.
 *
 * @param handle Handle of the attribute being set.
 * @param pData Pointer to the data holding the data value.
 * @param len Length of the data being set.
 */
void BluetoothManager::SetCurrentValueForAttributeHandle(uint16_t handle, uint8_t *pData, uint16_t len)
{
    TRACE_MESSAGE("SetCurrentValueForAttributeHandle: Enter");

    TRACE_MESSAGE("Received %i bytes of data for attribute handle 0x%04x", len, handle);

    auto existing = _valueMap.find(handle);

    if (existing != _valueMap.end())
    {
        // Clear and write
        memset(existing->second.value, 0, existing->second.length);
        // Only write *up to* the defined max length
        size_t length_to_write = existing->second.length < len ? existing->second.length : len;
        memcpy(existing->second.value, pData, length_to_write);
        TRACE_MESSAGE("Value length %i", length_to_write);
        TRACE_HEX_BUFFER(existing->second.value, length_to_write);
    }
    else
    {
        // Unknown handle - this should *never* happen, but best to be safe
        TRACE_MESSAGE("Value received with unknown handle 0x%04x", test_value);
    }

    TRACE_MESSAGE("SetCurrentValueForAttributeHandle: Exit");
}

/**
 * @brief Delete all of the registered services.
 *
 * @return bool True if the services were deleted, false otherwise.
 */
bool BluetoothManager::DeleteAllServices()
{
    for (auto val: _valueMap)
    {
        delete[] val.second.value;
    }
    _valueMap.clear();

    return Services.DeleteAllServices();
}

/**
 * @brief Build the graph of services, characteristics and descriptors from a pointer toJSON configuration.
 *
 * @param cfg_json JSON version of the service configuration.
 */
void BluetoothManager::BuildGraphFromJson(char *cfg_json)
{
    TRACE_MESSAGE("BuildGraphFromJson: Enter");

    Document doc;
    doc.Parse(cfg_json);

    Value &deviceName = doc["deviceName"];
    strcpy(_deviceName, deviceName.GetString());
    TRACE_MESSAGE("Device Name = %s", _deviceName);

    if (doc.HasMember("companyIdentifier"))
    {
        auto companyIdentifier = (uint16_t) doc["companyIdentifier"].GetInt();
        TRACE_MESSAGE("   companyIdentifier = 0x%04x", companyIdentifier);
        _companyIdentifier = companyIdentifier;
    }
    else
    {
        _companyIdentifier = 0xffff;
    }

    const Value &svc_list = doc["services"];
    for (auto i = 0; i < svc_list.Size(); i++)
    {
        auto svc = svc_list[i].GetObject();
        auto svc_uuid = svc["uuid"].GetInt();
        auto svc_name = svc["name"].GetString();
        TRACE_MESSAGE("service %s uuid = 0x%04x", svc_name, svc_uuid);

        auto bt_svc = new BluetoothService(svc_name, Uuid16(svc_uuid));

        const Value &char_list = svc["characteristics"];
        TRACE_MESSAGE("service has %i characteristics", char_list.Size());
        for (auto i = 0; i < char_list.Size(); i++)
        {
            TRACE_MESSAGE("characteristic ");
            auto characteristic = char_list[i].GetObject();

            // this might be a ushort, might be a guid

            Uuid16 chr_uuid16;
            Uuid128 chr_uuid128;
            auto is128 = false;

            if (characteristic["uuid"].IsInt())
            {
                chr_uuid16 = Uuid16((uint16_t) characteristic["uuid"].GetInt());
                TRACE_MESSAGE(" uuid = 0x%04x", (uint16_t) chr_uuid16);
            }
            else
            {
                chr_uuid128 = Uuid128(characteristic["uuid"].GetString());
                TRACE_MESSAGE(" uuid = %s", chr_uuid128.ToString().c_str());
                is128 = true;
            }

            auto chr_perm = (uint16_t) characteristic["permission"].GetInt();
            auto chr_props = (uint16_t) characteristic["props"].GetInt();
            auto chr_len = (uint16_t) characteristic["len"].GetInt();

            TRACE_MESSAGE(" permission = 0x%04x", chr_perm);
            TRACE_MESSAGE(" props = 0x%04x", chr_props);
            TRACE_MESSAGE(" len = 0x%04x", chr_len);

            BluetoothCharacteristic *bt_characteristic;
            if (is128)
            {
                bt_characteristic = new BluetoothCharacteristic(chr_uuid128, (CharacteristicPermission) chr_perm, (CharacteristicProperty) chr_props, chr_len);
            }
            else
            {
                bt_characteristic = new BluetoothCharacteristic(chr_uuid16, (CharacteristicPermission) chr_perm, (CharacteristicProperty) chr_props, chr_len);
            }

            if (characteristic.HasMember("descriptors"))
            {
                TRACE_MESSAGE("attribute has descriptors node");
                const Value &desc_list = characteristic["descriptors"];
                if (desc_list.Size() > 0)
                {
                    TRACE_MESSAGE("characteristic has %i descriptors", desc_list.Size());
                    for (auto d = 0; d < desc_list.Size(); d++)
                    {
                        Uuid16 desc_uuid16;
                        Uuid128 desc_uuid128;
                        auto is128d = false;
                        auto descriptor = desc_list[d].GetObject();

                        if (descriptor["uuid"].IsInt())
                        {
                            desc_uuid16 = Uuid16((uint16_t) descriptor["uuid"].GetInt());
                            TRACE_MESSAGE(" uuid = 0x%04x", (uint16_t) desc_uuid16);
                        }
                        else
                        {
                            desc_uuid128 = Uuid128(descriptor["uuid"].GetString());
                            TRACE_MESSAGE(" uuid = %s", desc_uuid128.ToString().c_str());
                            is128d = true;
                        }

                        auto desc_perm = (uint16_t) descriptor["permission"].GetInt();
                        TRACE_MESSAGE("   permission = 0x%04x", desc_perm);
                        auto desc_len = (uint16_t) descriptor["len"].GetInt();
                        TRACE_MESSAGE("   len = 0x%04x", desc_len);

                        auto desc_value = (uint16_t) descriptor["value"].GetInt();
                        TRACE_MESSAGE("   value = 0x%04x", desc_value);

                        BluetoothDescriptor *bt_descriptor;

                        if (is128d)
                        {
                            bt_descriptor = new BluetoothDescriptor(bt_characteristic, desc_uuid128, (CharacteristicPermission) desc_perm, desc_len);
                        }
                        else
                        {
                            bt_descriptor = new BluetoothDescriptor(bt_characteristic, desc_uuid16, (CharacteristicPermission) desc_perm, desc_len);
                        }

                        bt_descriptor->SetValue(desc_value);

                        TRACE_MESSAGE("bt_descriptor created");
                        bt_characteristic->AddDescriptor(bt_descriptor);
                        TRACE_MESSAGE("bt_descriptor added");
                    }
                }
            }

            bt_svc->AddCharacteristic(bt_characteristic);
            TRACE_MESSAGE("bt_characteristic added");
        }

        Services.Add(bt_svc);
    }

    TRACE_MESSAGE("BuildGraphFromJson: Exit");
}

/**
 * @brief Generate the attributes table for the GATT database.
 */
void BluetoothManager::GenerateAttributeTable()
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
    for (const auto &svc: Services.GetServiceDefinitions())
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

    TRACE_MESSAGE("ATTRIBUTE TABLE LENGTH = %d", count);

    _attribute_table = new esp_gatts_attr_db_t[count];
    _attributeTableLength = count;

    auto index = 0;
    for (const auto &svc: Services.GetServiceDefinitions())
    {
        TRACE_MESSAGE("ATTRIBUTE TABLE SERVICE AT %d ", index);
        // service attribute - all services are primary for now
        _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *) &primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(svc->Uuid), (uint8_t *) &svc->Uuid}};

        TRACE_MESSAGE("CONTAINS %d CHARACTERISTICS", svc->GetCharacteristicDefinitions().size());
        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            TRACE_MESSAGE("ATTRIBUTE TABLE DECLARATION AT %d", index);
            // declaration attribute
            _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *) &characteristic->Declaration->Uuid, (uint16_t) characteristic->Declaration->Permission, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *) &characteristic->Declaration->Properties}};

            TRACE_MESSAGE("ATTRIBUTE TABLE VALUE AT %d", index);
            // value attribute
            // the first param here (ESP_GATT_RSP_BY_APP) means that we'll handle the request for data manually rather than allowing the API to respond with
            // data in the parameter database.  We need to call to the STM for data, so this is sensible.
            if (characteristic->Value->Is128())
            {
                _attribute_table[index++] = {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_128, (uint8_t *) &characteristic->Value->uuid128, (uint16_t) characteristic->Value->Permission, characteristic->Value->MaxLength, 0, NULL}}; // <- 0, NULL means "current value length is 0, current value data is null"
            }
            else
            {
                _attribute_table[index++] = {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *) &characteristic->Value->uuid16, (uint16_t) characteristic->Value->Permission, characteristic->Value->MaxLength, 0, NULL}}; // <- 0, NULL means "current value length is 0, current value data is null"
            }

            for (const auto &descriptor: characteristic->GetDescriptorDefinitions())
            {
                TRACE_MESSAGE("ATTRIBUTE TABLE DESCRIPTOR AT %d", index);
                if (descriptor->Is128())
                {
                    _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *) &descriptor->uuid128, (uint16_t) descriptor->Permission, descriptor->MaxLength, descriptor->CurrentLength, (uint8_t *) descriptor->Value}};
                }
                else
                {
                    _attribute_table[index++] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *) &descriptor->uuid16, (uint16_t) descriptor->Permission, descriptor->MaxLength, descriptor->CurrentLength, (uint8_t *) descriptor->Value}};
                }
            }
        }
    }

    TRACE_MESSAGE("GenerateAttributeTable: Exit");
}
