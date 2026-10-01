/**
 * @file BluetoothManager.cpp
 * @author Chris Tacke
 * @brief Implementation of the BluetoothManager class.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#include "cJSON.h"
#include "esp_gatt_defs.h"

#include "Logging.hpp"

#include "Esp32Messaging.hpp"
#include "BluetoothManager.hpp"
#include "MutexLock.hpp"
#include "BluetoothComponentName.hpp"

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

    _graphMutex = xSemaphoreCreateRecursiveMutex();
    if (_graphMutex == nullptr)
    {
        ERROR_MESSAGE("BluetoothManager: cannot create the service graph mutex.");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

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

    TRACE_MESSAGE("New Service: 0x%04" PRIx16 " (%s)", service->Handle, service->Name.c_str());

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

    TRACE_MESSAGE("New Characteristic: declaration @ 0x%04" PRIx16 " value @ 0x%04" PRIx16, characteristic->Declaration->Handle, characteristic->Value->Handle);

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

    TRACE_MESSAGE("New Characteristic Value: value @ 0x%04" PRIx16 " is %" PRIu16 " bytes", characteristic->Value->Handle, characteristic->Value->CurrentLength);

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
 * @param handles Pointer to an array of uint16_t values to hold the handles.
 * @param capacity Number of uint16_t values that fit in handles.
 *
 * @return True if every handle in the service graph was written, false if the graph
 *         needed more than capacity handles and handles was left partially filled
 *         (issue #350).
 */
bool BluetoothManager::GetHandles(uint16_t * const handles, const uint16_t capacity)
{
    RecursiveMutexLock lock(_graphMutex);
    TRACE_MESSAGE("GetHandles: Enter");

    uint16_t index = 0;

    for (const auto &svc: Services.GetServiceDefinitions())
    {
        if (index >= capacity)
        {
            TRACE_MESSAGE("GetHandles: Exit - capacity %" PRIu16 " exceeded", capacity);
            return (false);
        }
        handles[index++] = svc->Handle;

        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            if ((index + 2) > capacity)
            {
                TRACE_MESSAGE("GetHandles: Exit - capacity %" PRIu16 " exceeded", capacity);
                return (false);
            }
            handles[index++] = characteristic->Declaration->Handle;
            handles[index++] = characteristic->Value->Handle;

            for (const auto &descriptor: characteristic->GetDescriptorDefinitions())
            {
                if (index >= capacity)
                {
                    TRACE_MESSAGE("GetHandles: Exit - capacity %" PRIu16 " exceeded", capacity);
                    return (false);
                }
                handles[index++] = descriptor->Handle;
            }
        }
    }

    TRACE_MESSAGE("GetHandles: Exit");
    return (true);
}

/**
 * @brief Get the properties for the characteristic with the specified handle.
 *
 * @param handle Handle for the characteristic
 * @return CharacteristicProperty Properties of the characteristic or 0 if the handle cannot be found.
 */
CharacteristicProperty BluetoothManager::GetCharacteristicProperties(const uint16_t handle)
{
    RecursiveMutexLock lock(_graphMutex);
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

    return static_cast<CharacteristicProperty>(0);
}

/**
 * @brief Check whether a handle identifies a known characteristic value.
 *
 *  A characteristic legitimately configured with no properties would make
 *  GetCharacteristicProperties() == 0 ambiguous between "not found" and "found, but
 *  nothing set"; this checks handle identity directly instead (issue #300).
 *
 * @param handle Handle to look up.
 *
 * @return True if a characteristic with this value handle exists, false otherwise.
 */
bool BluetoothManager::IsKnownCharacteristicHandle(const uint16_t handle)
{
    RecursiveMutexLock lock(_graphMutex);
    for (const auto &svc: Services.GetServiceDefinitions())
    {
        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            if (characteristic->Value->Handle == handle)
            {
                return (true);
            }
        }
    }

    return (false);
}

/**
 * @brief Get the permissions for the characteristic with the specified handle.
 *
 * @param handle Handle for the characteristic
 * @return CharacteristicPermission Permissions of the characteristic or 0 if the handle cannot be found.
 */
CharacteristicPermission BluetoothManager::GetCharacteristicPermissions(const uint16_t handle)
{
    RecursiveMutexLock lock(_graphMutex);
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

    return static_cast<CharacteristicPermission>(0);
}

/**
 * @brief Set the handles for the services, characteristics and descriptors.
 *
 * @param handles Pointer to a block of memory holding the handles for the services, characteristics and descriptors.
 * @param count Number of handles available in handles.
 *
 * @return True if every handle needed by the service graph was read, false if the graph
 *         needed more than count handles and was left partially updated (issue #350).
 */
bool BluetoothManager::SetHandles(const uint16_t * const handles, const uint16_t count)
{
    RecursiveMutexLock lock(_graphMutex);
    TRACE_MESSAGE("SetHandles: Enter");

    TRACE_MESSAGE("Handles: ");
    for (int i = 0; i < count; i++)
    {
        TRACE_MESSAGE("   0x%04x ", handles[i]);
    }

    uint16_t index = 0;

    for (const auto &svc: Services.GetServiceDefinitions())
    {
        if (index >= count)
        {
            TRACE_MESSAGE("SetHandles: Exit - handle count %" PRIu16 " exceeded", count);
            return (false);
        }
        svc->Handle = handles[index++];
        TRACE_MESSAGE("ATTRIBUTE TABLE SERVICE HANDLE = 0x%04" PRIx16, svc->Handle);

        for (const auto &characteristic: svc->GetCharacteristicDefinitions())
        {
            if ((index + 2) > count)
            {
                TRACE_MESSAGE("SetHandles: Exit - handle count %" PRIu16 " exceeded", count);
                return (false);
            }
            characteristic->Declaration->Handle = handles[index++];
            TRACE_MESSAGE("ATTRIBUTE TABLE DECLARATION HANDLE = 0x%04" PRIx16, characteristic->Declaration->Handle);

            characteristic->Value->Handle = handles[index++];
            TRACE_MESSAGE("ATTRIBUTE TABLE VALUE HANDLE = 0x%04" PRIx16 " | max length = %" PRIu16, characteristic->Value->Handle, characteristic->Value->MaxLength);
            auto val = length_val_s {.length = characteristic->Value->MaxLength, .value = new uint8_t[characteristic->Value->MaxLength]};
            memset(val.value, 0, characteristic->Value->MaxLength);

            TRACE_MESSAGE("Request for data for handle 0x%04" PRIx16, characteristic->Value->Handle);

            auto existingValue = _valueMap.find(characteristic->Value->Handle);
            if (existingValue != _valueMap.end())
            {
                delete[] existingValue->second.value;
            }
            _valueMap.insert_or_assign(characteristic->Value->Handle, val);
            // cross reference handles
            characteristic->Declaration->ValueHandle = characteristic->Value->Handle;

            for (const auto &descriptor: characteristic->GetDescriptorDefinitions())
            {
                if (index >= count)
                {
                    TRACE_MESSAGE("SetHandles: Exit - handle count %" PRIu16 " exceeded", count);
                    return (false);
                }
                descriptor->Handle = handles[index++];
                TRACE_MESSAGE("ATTRIBUTE TABLE DESCRIPTOR HANDLE = 0x%04" PRIx16, descriptor->Handle);
            }
        }
    }

    TRACE_MESSAGE("SetHandles: Exit");
    return (true);
}

/**
 * @brief Get the current value for a attribute with the specified handle.
 *
 * @param handle Handle to look up.
 * @param pData Pointer to a block of memory to take the value.
 * @param capacity Size of the block at pData; no more than this many bytes are copied.
 * @param pLen Pointer to a variable that will hold the length of the data that is being copied.
 *
 * @return true if the handle is known and pData/pLen were filled in, false if the handle
 *  is unknown, in which case pLen is set to 0 and pData is left untouched.
 */
bool BluetoothManager::GetCurrentValueForAttributeHandle(const uint16_t handle, uint8_t *pData, const uint16_t capacity, uint16_t * const pLen)
{
    RecursiveMutexLock lock(_graphMutex);
    TRACE_MESSAGE("GetCurrentValueForAttributeHandle: Enter");

    // TODO: go get the data from the STM - for now we just increment to test the data is changing
    auto existing = _valueMap.find(handle);
    bool found = (existing != _valueMap.end());

    if (found)
    {
        TRACE_MESSAGE("Request for data for handle 0x%04" PRIx16, handle);
        TRACE_MESSAGE(" known length = %" PRIu16, existing->second.length);

        uint16_t length = existing->second.length;
        if (length > capacity)
        {
            ERROR_MESSAGE("GetCurrentValueForAttributeHandle: value of %" PRIu16 " bytes for handle 0x%04" PRIx16 " truncated to %" PRIu16 ".", length, handle, capacity);
            length = capacity;
        }
        memcpy(pData, existing->second.value, length);
        *pLen = length;
    }
    else
    {
        TRACE_MESSAGE("Request for data with unknown handle 0x%04" PRIx16, handle);
        *pLen = 0;
    }

    TRACE_MESSAGE("GetCurrentValueForAttributeHandle: Exit");

    return (found);
}

/**
 * @brief Set the value for a attribute with the specified handle.
 *
 * @param handle Handle of the attribute being set.
 * @param pData Pointer to the data holding the data value.
 * @param len Length of the data being set.
 */
void BluetoothManager::SetCurrentValueForAttributeHandle(const uint16_t handle, const uint8_t * const pData, const uint16_t len)
{
    RecursiveMutexLock lock(_graphMutex);
    TRACE_MESSAGE("SetCurrentValueForAttributeHandle: Enter");

    TRACE_MESSAGE("Received %" PRIu16 " bytes of data for attribute handle 0x%04" PRIx16, len, handle);

    auto existing = _valueMap.find(handle);

    if (existing != _valueMap.end())
    {
        // Clear and write
        memset(existing->second.value, 0, existing->second.length);
        // Only write *up to* the defined max length
        uint16_t length_to_write = existing->second.length < len ? existing->second.length : len;
        memcpy(existing->second.value, pData, length_to_write);
        TRACE_MESSAGE("Value length %" PRIu16, length_to_write);
    }
    else
    {
        // Unknown handle - this should *never* happen, but best to be safe
        TRACE_MESSAGE("Value received with unknown handle 0x%02" PRIx16, handle);
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
    RecursiveMutexLock lock(_graphMutex);
    TRACE_MESSAGE("DeleteAllServices: Enter");

    for (const auto val: _valueMap)
    {
        delete[] val.second.value;
    }
    _valueMap.clear();

    TRACE_MESSAGE("DeleteAllServices: Exit");

    return Services.DeleteAllServices();
}

/**
 * @brief Get the device information from the JSON configuration.
 *
 * @param json Pointer to the cJSON object holding the configuration.
 */
void BluetoothManager::GetDeviceInformation(const cJSON * const json)
{
    TRACE_MESSAGE("GetDeviceInformation: Enter");

    const cJSON * const deviceName = cJSON_GetObjectItem(json, DEVICE_NAME_KEY);
    memset(_deviceName, 0, sizeof(_deviceName));
    if ((deviceName != nullptr) && cJSON_IsString(deviceName))
    {
        strncpy(_deviceName, deviceName->valuestring, sizeof(_deviceName) - 1);
    }
    else
    {
        strncpy(_deviceName, DEFAULT_DEVICE_NAME, sizeof(_deviceName) - 1);
    }
    TRACE_MESSAGE("Device Name: %s\n", _deviceName);

    const cJSON * const identifier = cJSON_GetObjectItem(json, COMPANY_IDENTIFIER_KEY);
    if ((identifier == nullptr) || !cJSON_IsNumber(identifier))
    {
        _companyIdentifier = DEBUG_COMPANY_IDENTIFIER;
    }
    else
    {
        _companyIdentifier = identifier->valueint;
    }
    TRACE_MESSAGE("Company identifier: 0x%04" PRIx16, _companyIdentifier);

    TRACE_MESSAGE("GetDeviceInformation: Exit");
}

/**
 * @brief Add services from the JSON configuration.
 *
 * Every entry is examined: a malformed service is counted and skipped rather than ending
 * the parse, so the caller learns how much of the configuration could not be used.
 *
 * @param json Pointer to the cJSON object holding the configuration.
 *
 * @return Number of service, characteristic and descriptor entries that were rejected.
 */
uint32_t BluetoothManager::AddServices(const cJSON * const json)
{
    TRACE_MESSAGE("AddServices: Enter");

    uint32_t rejected = 0;
    const cJSON * const services = cJSON_GetObjectItem(json, SERVICES_KEY);
    if ((services == nullptr) || !cJSON_IsArray(services))
    {
        TRACE_MESSAGE(SERVICES_KEY " is not an array or is missing");
        return (rejected);
    }

    cJSON *service_configuration = nullptr;
    cJSON_ArrayForEach(service_configuration, services)
    {
        const cJSON * const serviceName = cJSON_GetObjectItem(service_configuration, NAME_KEY);
        const cJSON * const serviceUUID = cJSON_GetObjectItem(service_configuration, UUID_KEY);
        if ((serviceName == nullptr) || !cJSON_IsString(serviceName))
        {
            ERROR_MESSAGE("Service " NAME_KEY " is missing or not a string, rejecting the service.");
            rejected++;
        }
        else
        {
            if ((serviceUUID == nullptr) || !cJSON_IsNumber(serviceUUID))
            {
                ERROR_MESSAGE("Service " UUID_KEY " is missing or not a number, rejecting the service.");
                rejected++;
            }
            else
            {
                uint16_t uuid = static_cast<uint16_t>(serviceUUID->valueint);
                TRACE_MESSAGE("Service Name: %s, " UUID_KEY ": %4" PRIx16, serviceName->valuestring, uuid);
                BluetoothService * const service = new BluetoothService(serviceName->valuestring, Uuid16(uuid));
                rejected += AddCharacteristics(service_configuration, service);
                Services.Add(service);
            }
        }
    }

    TRACE_MESSAGE("AddServices: Exit");
    return (rejected);
}

/**
 * @brief Add characteristics from the JSON configuration.
 *
 * @param json Pointer to the cJSON object holding the configuration.
 * @param service Pointer to the BluetoothService object to add the characteristics to.
 *
 * @return Number of characteristic and descriptor entries that were rejected.  A service
 *         with no characteristics key has none to reject; one that is not an array counts once.
 */
uint32_t BluetoothManager::AddCharacteristics(const cJSON * const json, BluetoothService * const service)
{
    TRACE_MESSAGE("AddCharacteristics: Enter");

    uint32_t rejected = 0;
    const cJSON * const characteristics = cJSON_GetObjectItem(json, CHARACTERISTICS_KEY);
    if (characteristics == nullptr)
    {
        TRACE_MESSAGE(CHARACTERISTICS_KEY " is missing");
        return (rejected);
    }
    if (!cJSON_IsArray(characteristics))
    {
        ERROR_MESSAGE(CHARACTERISTICS_KEY " is not an array, rejecting it.");
        rejected++;
        return (rejected);
    }

    cJSON *characteristic = nullptr;
    cJSON_ArrayForEach(characteristic, characteristics)
    {
        bool add = true;
        const cJSON * const permission = cJSON_GetObjectItem(characteristic, PERMISSION_KEY);
        if ((permission == nullptr) || !cJSON_IsNumber(permission))
        {
            TRACE_MESSAGE("Characteristic " PERMISSION_KEY " is missing or not a number");
            add = false;
        }
        const cJSON * const props = cJSON_GetObjectItem(characteristic, PROPERTIES_KEY);
        if ((props == nullptr) || !cJSON_IsNumber(props))
        {
            TRACE_MESSAGE("Characteristic " PROPERTIES_KEY " is missing or not a number");
            add = false;
        }
        const cJSON * const len = cJSON_GetObjectItem(characteristic, LENGTH_KEY);
        if ((len == nullptr) || !cJSON_IsNumber(len))
        {
            TRACE_MESSAGE("Characteristic " LENGTH_KEY " is missing or not a number");
            add = false;
        }
        else
        {
            //
            //  The length sizes the value's storage and is copied into a GATT response
            //  buffer of ESP_GATT_MAX_ATTR_LEN bytes when a peer reads it, so it has to fit.
            //
            if ((len->valueint < 1) || (len->valueint > ESP_GATT_MAX_ATTR_LEN))
            {
                ERROR_MESSAGE("Characteristic " LENGTH_KEY " %d is outside 1 to %d, rejecting it.", len->valueint, ESP_GATT_MAX_ATTR_LEN);
                add = false;
            }
        }
        const cJSON * const uuid = cJSON_GetObjectItem(characteristic, UUID_KEY);
        if ((uuid == nullptr) || !(cJSON_IsNumber(uuid) || cJSON_IsString(uuid)))
        {
            TRACE_MESSAGE("Characteristic " UUID_KEY " is missing or not a number or string");
            add = false;
        }

        if (add)
        {
            Uuid16 uuid16bits;
            Uuid128 uuid128bits;
            bool is128bits = false;
            ParseUuid(uuid, uuid16bits, uuid128bits, is128bits);

            uint16_t permissions = static_cast<uint16_t>(permission->valueint);
            uint16_t properties = static_cast<uint16_t>(props->valueint);
            uint16_t length = static_cast<uint16_t>(len->valueint);

            BluetoothCharacteristic *new_characteristic;
            if (is128bits)
            {
                new_characteristic = new BluetoothCharacteristic(uuid128bits, static_cast<CharacteristicPermission>(permissions), static_cast<CharacteristicProperty>(properties), length);
            }
            else
            {
                new_characteristic = new BluetoothCharacteristic(uuid16bits, static_cast<CharacteristicPermission>(permissions), static_cast<CharacteristicProperty>(properties), length);
            }

            char buffer[40];
            memset(buffer, 0, sizeof(buffer));
            if (is128bits)
            {
                strncpy(buffer, uuid128bits.ToString().c_str(), sizeof(buffer) - 1);
            }
            else
            {
                snprintf(buffer, sizeof(buffer), "0x%04" PRIx16, static_cast<uint16_t>(uuid16bits));
            }
            TRACE_MESSAGE("    Characteristic : " UUID_KEY " %s, " PERMISSION_KEY ": %d, " PROPERTIES_KEY ": %d, " LENGTH_KEY ": %d", buffer, permissions, properties, length);

            rejected += AddDescriptors(characteristic, new_characteristic);
            service->AddCharacteristic(new_characteristic);
        }
        else
        {
            ERROR_MESSAGE("Rejecting a malformed characteristic.");
            rejected++;
        }
    }

    TRACE_MESSAGE("AddCharacteristics: Exit");
    return (rejected);
}

/**
 * @brief Add descriptors from the JSON configuration.
 *
 * @param json Pointer to the cJSON object holding the configuration.
 * @param characteristic Pointer to the BluetoothCharacteristic object to add the descriptors to.
 *
 * @return Number of descriptor entries that were rejected.  A characteristic with no
 *         descriptors key has none to reject; one that is not an array counts once.
 */
uint32_t BluetoothManager::AddDescriptors(const cJSON * const json, BluetoothCharacteristic * const characteristic)
{
    TRACE_MESSAGE("AddDescriptors: Enter");

    uint32_t rejected = 0;
    const cJSON * const descriptors = cJSON_GetObjectItem(json, DESCRIPTORS_KEY);
    if (descriptors == nullptr)
    {
        TRACE_MESSAGE(DESCRIPTORS_KEY " is missing");
        return (rejected);
    }
    if (!cJSON_IsArray(descriptors))
    {
        ERROR_MESSAGE(DESCRIPTORS_KEY " is not an array, rejecting it.");
        rejected++;
        return (rejected);
    }

    cJSON *descriptor = nullptr;
    cJSON_ArrayForEach(descriptor, descriptors)
    {
        bool add = true;
        const cJSON * const permission = cJSON_GetObjectItem(descriptor, PERMISSION_KEY);
        if ((permission == nullptr) || !cJSON_IsNumber(permission))
        {
            TRACE_MESSAGE("Descriptor " PERMISSION_KEY " is missing or not a number");
            add = false;
        }
        const cJSON * const uuid = cJSON_GetObjectItem(descriptor, UUID_KEY);
        if ((uuid == nullptr) || !(cJSON_IsNumber(uuid) || cJSON_IsString(uuid)))
        {
            TRACE_MESSAGE("Descriptor " UUID_KEY " is missing or not a number or string");
            add = false;
        }
        const cJSON * const len = cJSON_GetObjectItem(descriptor, LENGTH_KEY);
        if ((len == nullptr) || !cJSON_IsNumber(len))
        {
            TRACE_MESSAGE("Descriptor " LENGTH_KEY " is missing or not a number");
            add = false;
        }
        else
        {
            //
            //  The length sizes the value's storage and is copied into a GATT response
            //  buffer of ESP_GATT_MAX_ATTR_LEN bytes when a peer reads it, so it has to fit.
            //
            if ((len->valueint < 1) || (len->valueint > ESP_GATT_MAX_ATTR_LEN))
            {
                ERROR_MESSAGE("Descriptor " LENGTH_KEY " %d is outside 1 to %d, rejecting it.", len->valueint, ESP_GATT_MAX_ATTR_LEN);
                add = false;
            }
        }
        const cJSON * const value = cJSON_GetObjectItem(descriptor, VALUE_KEY);
        if ((value == nullptr) || !(cJSON_IsNumber(value) || cJSON_IsString(value)))
        {
            TRACE_MESSAGE("Descriptor " VALUE_KEY " is missing or not a number or string");
            add = false;
        }

        if (add)
        {
            Uuid16 uuid16bits;
            Uuid128 uuid128bits;
            bool is128bits = false;
            ParseUuid(uuid, uuid16bits, uuid128bits, is128bits);

            uint16_t permissions = static_cast<uint16_t>(permission->valueint);
            uint16_t length = static_cast<uint16_t>(len->valueint);

            /*
             *  BluetoothDescriptor's constructor copies MaxLength bytes starting at the
             *  pointer we give it, so that pointer has to reference length real bytes of
             *  descriptor content, zero-padded if the content is shorter.  Taking the
             *  address of a cJSON struct member is not that: it hands the constructor the
             *  field's own storage (or, for valuestring, the address of the pointer itself)
             *  and lets it read whatever heap memory follows.
             */
            uint8_t * const valueBuffer = new uint8_t[length];
            memset(valueBuffer, 0, length);
            if (cJSON_IsString(value))
            {
                size_t contentLength = strlen(value->valuestring);
                if (contentLength > length)
                {
                    contentLength = length;
                }
                memcpy(valueBuffer, value->valuestring, contentLength);
            }
            else
            {
                int numericValue = value->valueint;
                size_t contentLength = sizeof(numericValue);
                if (contentLength > length)
                {
                    contentLength = length;
                }
                memcpy(valueBuffer, &numericValue, contentLength);
            }

            BluetoothDescriptor *new_descriptor;
            if (is128bits)
            {
                new_descriptor = new BluetoothDescriptor(characteristic, uuid128bits, static_cast<CharacteristicPermission>(permissions), length, valueBuffer, length);
            }
            else
            {
                new_descriptor = new BluetoothDescriptor(characteristic, uuid16bits, static_cast<CharacteristicPermission>(permissions), length, valueBuffer, length);
            }
            delete[] valueBuffer;

            char buffer[40];
            memset(buffer, 0, sizeof(buffer));
            if (is128bits)
            {
                strncpy(buffer, uuid128bits.ToString().c_str(), sizeof(buffer) - 1);
            }
            else
            {
                snprintf(buffer, sizeof(buffer), "0x%04" PRIx16, static_cast<uint16_t>(uuid16bits));
            }
            TRACE_MESSAGE("        Descriptor : " UUID_KEY " %s, " PERMISSION_KEY ": %d, " LENGTH_KEY ": %d", buffer, permissions, length);

            characteristic->AddDescriptor(new_descriptor);
        }
        else
        {
            ERROR_MESSAGE("Rejecting a malformed descriptor.");
            rejected++;
        }
    }

    TRACE_MESSAGE("AddDescriptors: Exit");
    return (rejected);
}

/**
 * @brief Parse UUID from cJSON object.
 *
 * @note This method assumes that the validity of the json has already been established and that
 *       json is a number or a string.
 *
 * @param json cJSON object containing the UUID.
 * @param uuid16 Reference to Uuid16 to be set if UUID is 16-bit.
 * @param uuid128 Reference to Uuid128 to be set if UUID is 128-bit.
 * @param is128 Reference to bool to indicate if UUID is 128-bit.
 */
void BluetoothManager::ParseUuid(const cJSON * const json, Uuid16 &uuid16, Uuid128 &uuid128, bool &is128)
{
    TRACE_MESSAGE("ParseUuid: Enter");

    if (cJSON_IsNumber(json))
    {
        Uuid16 uuid16bits = Uuid16(static_cast<uint16_t>(json->valueint));
        memcpy(uuid16.data, uuid16bits.data, sizeof(uuid16.data));
        is128 = false;
    }
    else
    {
        Uuid128 uuid128bits = Uuid128(json->valuestring);
        memcpy(uuid128.data, uuid128bits.data, sizeof(uuid128.data));
        is128 = true;
    }

    TRACE_MESSAGE("ParseUuid: Exit");
}

/**
 * @brief Build the graph of services, characteristics and descriptors from a pointer toJSON configuration.
 *
 * @param configuration JSON version of the Bluetooth configuration.
 *
 * @returns
 *      true if configuration was parsed and the graph was built from it, false if
 *      configuration was refused.
 */
bool BluetoothManager::BuildGraphFromJson(const char * const configuration)
{
    RecursiveMutexLock lock(_graphMutex);
    TRACE_MESSAGE("BuildGraphFromJson: Enter");

    bool result = false;

    if ((configuration == nullptr) || (strnlen(configuration, Message::MAXIMUM_PAYLOAD_LENGTH) >= Message::MAXIMUM_PAYLOAD_LENGTH))
    {
        TRACE_MESSAGE("BuildGraphFromJson: Refused - configuration is missing or not shorter than the maximum payload length");
    }
    else
    {
        cJSON * const json = cJSON_Parse(configuration);
        if (json == nullptr)
        {
            const char *error_location = cJSON_GetErrorPtr();
            if (error_location != nullptr)
            {
                TRACE_MESSAGE("BuildGraphFromJson: Error before '%s'", error_location);
            }
        }
        else
        {
            GetDeviceInformation(json);
            //
            //  A configuration with any entry that could not be used, or with no usable
            //  service at all, is refused as a whole rather than started in part: the
            //  Start response has no way to say which entries were dropped.  What was built
            //  is released so a later Start does not add to it.
            //
            const uint32_t rejected = AddServices(json);
            if ((rejected > 0) || Services.GetServiceDefinitions().empty())
            {
                ERROR_MESSAGE("BuildGraphFromJson: %" PRIu32 " entries rejected, %" PRIu32 " services built; refusing the configuration.", rejected, static_cast<uint32_t>(Services.GetServiceDefinitions().size()));
                DeleteAllServices();
            }
            else
            {
                result = true;
            }
        }
        //
        //  The JSON library detects nullptr and will simply return without action.
        //
        cJSON_Delete(json);
    }

    TRACE_MESSAGE("BuildGraphFromJson: Exit");
    return (result);
}