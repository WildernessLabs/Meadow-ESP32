/**
 * @file BluetoothManager.cpp
 * @author Chris Tacke
 *
 * @brief Implementation of the methods in the BluetoothManager class.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#include "Logging.hpp"

#include "BluetoothManager.hpp"
#include "BluetoothComponentName.hpp"

#include "../rapidjson/document.h"

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
const char *BluetoothManager::COMPONENT_NAME = BLUETOOTH_COMPONENT_NAME;

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Construct a new BluetoothManager object.
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
 *
 * @param descriptor Descriptor being added.
 */
void BluetoothManager::HandleDescriptorAdded(BluetoothDescriptor *descriptor)
{
    TRACE_MESSAGE("HandleDescriptorAdded: Enter");

    TRACE_MESSAGE("HandleDescriptorAdded: Exit");
}

/**
 * @brief Get all of the handles associated with the services, characteristics and descriptors.
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
 * @brief Get the properties for the characteristic with the specified handle.
 *
 * @param handle Handle for the characteristic
 * @return CharacteristicPermission Properties of the characteristic or 0 if the handle cannot be found.
 */
CharacteristicProperty BluetoothManager::GetCharacteristicProperties(uint16_t handle)
{
    for (const auto &svc: Services.GetServiceDefinitions())
    {
        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            if (characteristic->Value->Handle == handle)
            {
                return characteristic->Declaration->Properties;
            }
        }
    }

    return (CharacteristicProperty) 0;
}

/**
 * @brief Get the permissions for the characteristic with the specified handle.
 *
 * @param handle Handle for the characteristic
 * @return CharacteristicPermission Permissions of the characteristic or 0 if the handle cannot be found.
 */
CharacteristicPermission BluetoothManager::GetCharacteristicPermissions(uint16_t handle)
{
    for (const auto &svc: Services.GetServiceDefinitions())
    {
        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            if (characteristic->Value->Handle == handle)
            {
                return characteristic->Declaration->Permission;
            }
        }
    }

    return (CharacteristicPermission) 0;
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
 * @brief Dummy variable.
 */
static int __attribute__((unused)) test_value = 0;

/**
 * @brief Get the current value for a attribute with the specified handle.
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
 * @brief Set the value for a attribute with the specified handle.
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
        TRACE_MESSAGE("Service %s uuid = 0x%04x", svc_name, svc_uuid);

        auto bt_svc = new BluetoothService(svc_name, Uuid16(svc_uuid));

        const Value &char_list = svc["characteristics"];
        TRACE_MESSAGE("Service has %i characteristics", char_list.Size());
        for (auto i = 0; i < char_list.Size(); i++)
        {
            TRACE_MESSAGE("Characteristic ");
            auto characteristic = char_list[i].GetObject();

            // this might be a ushort, might be a guid

            Uuid16 chr_uuid16;
            Uuid128 chr_uuid128;
            auto is128 = false;

            if (characteristic["uuid"].IsInt())
            {
                chr_uuid16 = Uuid16((uint16_t) characteristic["uuid"].GetUint());
                TRACE_MESSAGE("    UUID = 0x%04x", chr_uuid16.ToUint16());
            }
            else
            {
                chr_uuid128 = Uuid128(characteristic["uuid"].GetString());
                TRACE_MESSAGE("    UUID = %s", chr_uuid128.ToString().c_str());
                is128 = true;
            }

            auto chr_perm = (uint16_t) characteristic["permission"].GetInt();
            auto chr_props = (uint16_t) characteristic["props"].GetInt();
            auto chr_len = (uint16_t) characteristic["len"].GetInt();

            TRACE_MESSAGE("    Permission = 0x%04x", chr_perm);
            TRACE_MESSAGE("    Props = 0x%04x", chr_props);
            TRACE_MESSAGE("    Len = 0x%04x", chr_len);

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
                TRACE_MESSAGE("Attribute has descriptors node");
                const Value &desc_list = characteristic["descriptors"];
                if (desc_list.Size() > 0)
                {
                    TRACE_MESSAGE("Characteristic has %i descriptors", desc_list.Size());
                    for (auto d = 0; d < desc_list.Size(); d++)
                    {
                        Uuid16 desc_uuid16;
                        Uuid128 desc_uuid128;
                        auto is128d = false;
                        auto descriptor = desc_list[d].GetObject();

                        if (descriptor["uuid"].IsInt())
                        {
                            desc_uuid16 = Uuid16((uint16_t) descriptor["uuid"].GetUint());
                            TRACE_MESSAGE("    UUID = 0x%04x", desc_uuid16.ToUint16());
                        }
                        else
                        {
                            desc_uuid128 = Uuid128(descriptor["uuid"].GetString());
                            TRACE_MESSAGE("     UUID = %s", desc_uuid128.ToString().c_str());
                            is128d = true;
                        }

                        auto desc_perm = (uint16_t) descriptor["permission"].GetInt();
                        TRACE_MESSAGE("    Permission = 0x%04x", desc_perm);
                        auto desc_len = (uint16_t) descriptor["len"].GetInt();
                        TRACE_MESSAGE("    Len = 0x%04x", desc_len);

                        uint32_t int_value = 0;
                        const char *str_value = nullptr;
                        uint8_t *value = nullptr;
                        if (descriptor["value"].IsInt())
                        {
                            int_value = (uint32_t) descriptor["value"].GetInt();
                            TRACE_MESSAGE("    Value = 0x%04x", int_value);
                            value = (uint8_t *) &int_value;
                        }
                        else
                        {
                            str_value = descriptor["value"].GetString();
                            TRACE_MESSAGE("    Value = %s", str_value);
                            value = (uint8_t *) str_value;
                        }

                        BluetoothDescriptor *bt_descriptor;

                        if (is128d)
                        {
                            bt_descriptor = new BluetoothDescriptor(bt_characteristic, desc_uuid128, (CharacteristicPermission) desc_perm, desc_len, value);
                        }
                        else
                        {
                            bt_descriptor = new BluetoothDescriptor(bt_characteristic, desc_uuid16, (CharacteristicPermission) desc_perm, desc_len, value);
                        }
                        bt_characteristic->AddDescriptor(bt_descriptor);
                        TRACE_MESSAGE("Bluetooth descriptor added");
                    }
                }
            }

            bt_svc->AddCharacteristic(bt_characteristic);
            TRACE_MESSAGE("Bluetooth characteristic added");
        }

        Services.Add(bt_svc);
    }

    TRACE_MESSAGE("BuildGraphFromJson: Exit");
}