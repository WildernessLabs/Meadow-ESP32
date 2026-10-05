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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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
 * @brief 16-bit UUID of the Client Characteristic Configuration descriptor (CCCD).
 */
#define CLIENT_CHARACTERISTIC_CONFIGURATION_UUID 0x2902

/**
 * @brief Length, in bytes, of the Client Characteristic Configuration descriptor's value.
 */
#define CLIENT_CHARACTERISTIC_CONFIGURATION_LENGTH 2

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
     *  @brief Recursive mutex guarding _valueMap and the handles in Services.
     *
     *  The value map and the handles are used both on the Bluetooth task, as requests from
     *  the STM32 arrive, and on the Bluedroid task, as a client reads a value or the stack
     *  hands back the attribute table's handles.  Recursive, as BuildGraphFromJson calls
     *  DeleteAllServices while holding it (issue #168).
     */
    SemaphoreHandle_t _graphMutex;

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
     *
     * @return true if the device name can be advertised, false if it is longer than
     *         MAXIMUM_ADVERTISED_NAME_LENGTH, in which case the default name is kept.
     */
    bool GetDeviceInformation(const cJSON * const json);

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
     * @brief Give a notifying or indicating characteristic a Client Characteristic Configuration descriptor.
     *
     *  A client subscribes by writing this descriptor, so without one CoreBluetooth reports
     *  "the attribute could not be found" and never delivers the value (issue #496).  A
     *  descriptor the configuration declares is kept; if its length is wrong it was corrected
     *  by AddDescriptors.
     *
     * @param characteristic Characteristic that has had its configured descriptors added.
     * @param properties Properties the configuration gave the characteristic.
     */
    void AddClientCharacteristicConfiguration(BluetoothCharacteristic * const characteristic, const uint16_t properties);

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

public:
    /**
     * @brief Longest device name, in bytes, that a configuration may give.
     *
     *  The name is advertised on its own in the scan response, which is 31 bytes, and
     *  the name's length and type bytes take 2 of them.  A longer name was either sent
     *  shortened or, past Bluedroid's own limit, refused by esp_ble_gap_set_device_name
     *  with nothing reported to the STM32 (issue #344).
     */
    static constexpr size_t MAXIMUM_ADVERTISED_NAME_LENGTH = 29;

private:
    /**
     * @brief Name of the device, with room for the null terminator.
     */
    char _deviceName[MAXIMUM_ADVERTISED_NAME_LENGTH + 1];

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
     * @brief Is this the handle of a characteristic's value?
     *
     *  Characteristic values are answered by the firmware (ESP_GATT_RSP_BY_APP).
     *  Descriptors are answered by Bluedroid, which still reports their writes.
     *
     * @param handle Handle to look up.
     *
     * @return true if handle holds a characteristic's value.
     */
    bool IsCharacteristicValueHandle(const uint16_t handle);

    /**
     * @brief Get the current value for a attribute with the specified handle, starting at an offset.
     *
     * @return ESP_GATT_OK if the handle is known and pData/pLen were filled in,
     *  ESP_GATT_INVALID_HANDLE if the handle is unknown, ESP_GATT_INVALID_OFFSET if the
     *  offset is beyond the length of the value.
     */
    esp_gatt_status_t GetCurrentValueForAttributeHandle(const uint16_t handle, const uint16_t offset, uint8_t *pData, const uint16_t capacity, uint16_t * const pLen);

    /**
     * @brief @brief Set the value for a attribute with the specified handle.
     */
    void SetCurrentValueForAttributeHandle(const uint16_t handle, const uint8_t * const pData, const uint16_t len);

    /**
     * @brief Get the handle of the Client Characteristic Configuration descriptor of the characteristic whose value has this handle.
     *
     *  Every characteristic that notifies or indicates has one (issue #496).
     *
     * @param valueHandle Handle of the characteristic's value.
     *
     * @return Handle of the descriptor, or 0 if the handle is not a characteristic value or the characteristic has none.
     */
    uint16_t GetClientConfigurationHandle(const uint16_t valueHandle);

    /**
     * @brief Get the handles of every Client Characteristic Configuration descriptor in the graph.
     *
     * @return The handles, in the order the characteristics were configured.
     */
    std::vector<uint16_t> GetClientConfigurationHandles();

    /**
     * @brief Does this Client Characteristic Configuration value ask for notifications?
     *
     *  Bit 0 enables notifications and bit 1 indications.  A server sends a notification
     *  only to a client that has set bit 0 (issue #572).
     *
     * @param configuration The two byte value a client wrote, as a little endian number.
     *
     * @return true if bit 0 is set.
     */
    static bool ClientConfigurationEnablesNotifications(const uint16_t configuration)
    {
        return ((configuration & 0x0001) != 0);
    }

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