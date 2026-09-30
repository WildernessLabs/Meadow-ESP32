/**
 * @file BluetoothManager.hpp
 * @author Chris Tacke
 * @brief Class definition for the BluetoothManager class.
 * @date 2021-02-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#pragma once

#include "cJSON.h"

#include "BluetoothServiceCollection.hpp"
#include "BluetoothAttribute.hpp"
#include "BluetoothHandleManager.hpp"
#include <vector>
#include <map>

/**
 * @brief Key name for device name in the JSON file.
 */
#define DEVICE_NAME_KEY "deviceName"

/**
 * @brief Default device name if none is specified.
 */
#define DEFAULT_DEVICE_NAME "MeadowF7"

/**
 * @brief Key name for company identifier in the JSON file.
 */
#define COMPANY_IDENTIFIER_KEY "companyIdentifier"

/**
 * @brief Key name for services in the JSON file.
 */
#define SERVICES_KEY "services"

/**
 * @brief Key name for characteristics in the JSON file.
 */
#define CHARACTERISTICS_KEY "characteristics"

/**
 * @brief Key name for name in the JSON file.
 */
#define NAME_KEY "name"

/**
 * @brief Key name for UUID in the JSON file.
 */
#define UUID_KEY "uuid"

/**
 * @brief Key name for permission in the JSON file.
 */
#define PERMISSION_KEY "permission"

/**
 * @brief Key name for properties in the JSON file.
 */
#define PROPERTIES_KEY "props"

/**
 * @brief Key name for length in the JSON file.
 */
#define LENGTH_KEY "len"

/**
 * @brief Key name for descriptors in the JSON file.
 */
#define DESCRIPTORS_KEY "descriptors"

/**
 * @brief Key name for values in the JSON file.
 */
#define VALUE_KEY "value"

/**
 * @brief Class holding the data and methods for the BluetoothManager.
 */
class BluetoothManager
{
private:
    /**
     * @brief Hold information about a value (length and pointer to the value).
     */
    struct length_val_s
    {
        /**
         * @brief Size of the memory block holding the value.
         */
        uint16_t length;
        /**
         * @brief Pointer to the memory holding the value.
         */
        uint8_t *value;
    };

    /**
     * @brief Type definition for the length_val_s structure.
     */
    typedef struct length_val_s length_val_t;

    /**
     * @brief Handle manager for the BluetoothManager.
     */
    BluetoothHandleManager _handleManager;

    /**
     * @brief Map of the values for the BluetoothManager along with their IDs.
     */
    std::map<uint16_t, length_val_s> _valueMap;

    /**
     * @brief Debug company identifier if none is specified.
     */
    const uint16_t DEBUG_COMPANY_IDENTIFIER = 0xffff;

    /**
     * @brief Process the addition of a new service.
     */
    static void HandleServiceAdded(BluetoothService *);

    /**
     * @brief Process the addition of a new characteristic.
     */
    static void HandleCharacteristicAdded(BluetoothCharacteristic *);

    /**
     * @brief Process the characteristic value changed event.
     */
    static void HandleValueChanged(BluetoothCharacteristic *);

    /**
     * @brief Handle the addition of a new descriptor.
     */
    static void HandleDescriptorAdded(BluetoothDescriptor *);

    /**
     * @brief Get the device information from the JSON configuration.
     *
     * @param json Pointer to the cJSON object holding the configuration.
     */
    void GetDeviceInformation(const cJSON * const json);

    /**
     * @brief Add services from the JSON configuration.
     *
     * @param json Pointer to the cJSON object holding the configuration.
     *
     * @return Number of service, characteristic and descriptor entries rejected.
     */
    uint32_t AddServices(const cJSON * const json);

    /**
     * @brief Add characteristics from the JSON configuration.
     *
     * @param json Pointer to the cJSON object holding the configuration.
     * @param service Pointer to the service to add the characteristics to.
     *
     * @return Number of characteristic and descriptor entries rejected.
     */
    uint32_t AddCharacteristics(const cJSON * const json, BluetoothService * const service);

    /**
     * @brief Add descriptors from the JSON configuration.
     *
     * @param json Pointer to the cJSON object holding the configuration.
     * @param characteristic Pointer to the BluetoothCharacteristic object to add the descriptors to.
     *
     * @return Number of descriptor entries rejected.
     */
    uint32_t AddDescriptors(const cJSON * const json, BluetoothCharacteristic * const characteristic);

    /**
     * @brief Parse UUID from cJSON object.
     *
     * @param json cJSON object containing the UUID.
     * @param uuid16 Reference to Uuid16 to be set if UUID is 16-bit.
     * @param uuid128 Reference to Uuid128 to be set if UUID is 128-bit.
     * @param is128 Reference to bool to indicate if UUID is 128-bit.
     */
    void ParseUuid(const cJSON * const json, Uuid16 &uuid16, Uuid128 &uuid128, bool &is128);

    /**
     * @brief 16-bit assigned number for the company identifier.
     */
    uint16_t _companyIdentifier;

    /**
     * @brief Maximum length, in bytes including the null terminator, of the device name.
     */
    static constexpr size_t MAXIMUM_DEVICE_NAME_LENGTH = 256;

    /**
     * @brief Name of the device.
     */
    char _deviceName[MAXIMUM_DEVICE_NAME_LENGTH];

public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Construct a new Bluetooth Manager object
     */
    BluetoothManager();

    /**
     * @brief Collection of services for this device.
     */
    BluetoothServiceCollection Services;

    /**
     * @brief Delete all of the registered services.
     */
    bool DeleteAllServices();

    /**
     * @brief Set the handles for the services, characteristics and descriptors.
     *
     * @param handles Block of memory holding the handles, count entries long.
     * @param count Number of handles available in handles.
     *
     * @return True if every handle needed by the service graph was read from handles,
     *         false if the graph needed more handles than count and was left partially
     *         updated.
     */
    bool SetHandles(const uint16_t * const handles, const uint16_t count);

    /**
     * @brief Get all of the handles associated with the services, characteristics and descriptors.
     *
     * @param handles Block of memory to hold the handles, capacity entries long.
     * @param capacity Number of handles that will fit in handles.
     *
     * @return True if every handle in the service graph was written to handles, false if
     *         the graph needed more handles than capacity and handles was left partially
     *         filled.
     */
    bool GetHandles(uint16_t * const handles, const uint16_t capacity);

    /**
     * @brief Get the Company Identifier.
     */
    uint16_t GetCompanyIdentifier()
    {
        return _companyIdentifier;
    }

    /**
     * @brief Get the device name.
     */
    const char *GetDeviceName() const
    {
        return (_deviceName);
    }

    /**
     * @brief Get the services collections.
     */
    const std::vector<BluetoothService *> GetServiceDefinitions()
    {
        return Services.GetServiceDefinitions();
    }

    /**
     * @brief Get the current value for a attribute with the specified handle.
     *
     * @return true if the handle is known and pData/pLen were filled in, false if the
     *  handle is unknown.
     */
    bool GetCurrentValueForAttributeHandle(const uint16_t handle, uint8_t *pData, const uint16_t capacity, uint16_t * const pLen);

    /**
     * @brief @brief Set the value for a attribute with the specified handle.
     */
    void SetCurrentValueForAttributeHandle(const uint16_t handle, const uint8_t * const pData, const uint16_t len);

    /**
     * @brief Get the properties for the characteristic with the specified handle.
     */
    CharacteristicProperty GetCharacteristicProperties(const uint16_t handle);

    /**
     * @brief Check whether a handle identifies a known characteristic value.
     *
     * @param handle Handle to look up.
     *
     * @return True if a characteristic with this value handle exists, false otherwise.
     */
    bool IsKnownCharacteristicHandle(const uint16_t handle);

    /**
     * @brief Get the permissions for the characteristic with the specified handle.
     */
    CharacteristicPermission GetCharacteristicPermissions(const uint16_t handle);

    /**
     * @brief Build the graph of services, characteristics and descriptors from a pointer toJSON configuration.
     *
     *  configuration is refused, and nothing is parsed, when it is null or its length is
     *  not less than Message::MAXIMUM_PAYLOAD_LENGTH, the documented bound on a fully
     *  reassembled message payload.  Without this check, cJSON_Parse would be handed a
     *  document bounded by nothing but the SPI payload itself, and an arbitrarily large
     *  one could exhaust the heap before any of the post-parse service and characteristic
     *  count limits get a chance to run.
     *
     * @param configuration JSON version of the Bluetooth configuration.
     *
     * @returns
     *      true if configuration was parsed and the graph was built from it, false if
     *      configuration was refused.
     */
    bool BuildGraphFromJson(const char * const configuration);
};