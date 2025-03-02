/**
 * @file BluetoothManager.hpp
 * @author Chris Tacke
 *
 * @brief Class definition for the BluetoothManager class.
 *
 * @copyright Copyright (c) 2019 - 2024
 */
#ifndef _BLUETOOTH_MANAGER_HPP_
#define _BLUETOOTH_MANAGER_HPP_

#include "SharedEnums.hpp"
#include "BluetoothServiceCollection.hpp"
#include "BluetoothAttribute.hpp"
#include "BluetoothHandleManager.hpp"
#include <vector>
#include <map>

#include "../rapidjson/document.h"

using namespace std;
using namespace rapidjson;

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
     * @brief Get the value for the specified node in the JSON document.
     */
    static int32_t GetJsonIntValue(Document &document, const char *node, int defaultValue);

    /**
     * @brief 16-bit assigned number for the company identifier.
     */
    uint16_t _companyIdentifier;

    /**
     * @brief Authorisation requirements for the device.
     */
    BluetoothAuthorisationRequirements::AuthorisationRequirements _authorisationRequirements;

    /**
     * @brief Passkey for the device.
     */
    uint32_t _passKey;

    /**
     * @brief Input / output capabilities for the device.
     */
    BluetoothIOCapabilities::Capabilities _ioCapabilities;

public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Name of the device.
     */
    char _deviceName[256];

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
     */
    void SetHandles(uint16_t *handles, uint16_t count);

    /**
     * @brief Get all of the handles associated with the services, characteristics and descriptors.
     */
    void GetHandles(uint16_t *handles);

    /**
     * @brief Get the Company Identifier.
     */
    uint16_t GetCompanyIdentifier()
    {
        return _companyIdentifier;
    }

    /**
     * @brief Get the Authorisation Requirements object
     */
    BluetoothAuthorisationRequirements::AuthorisationRequirements GetAuthorisationRequirements()
    {
        return _authorisationRequirements;
    }

    /**
     * @brief Get the pass key for the device.
     */
    uint32_t GetPassKey()
    {
        return _passKey;
    }

    /**
     * @brief Get the IO capabilities for the device.
     */
    BluetoothIOCapabilities::Capabilities GetIOCapabilities()
    {
        return _ioCapabilities;
    }

    /**
     * @brief Get the services collections.
     */
    const vector<BluetoothService *> GetServiceDefinitions()
    {
        return Services.GetServiceDefinitions();
    }

    /**
     * @brief Get the current value for a attribute with the specified handle.
     */
    void GetCurrentValueForAttributeHandle(uint16_t handle, uint8_t *pData, uint16_t *pLen);

    /**
     * @brief @brief Set the value for a attribute with the specified handle.
     */
    void SetCurrentValueForAttributeHandle(uint16_t handle, uint8_t *pData, uint16_t len);

    /**
     * @brief Get the properties for the characteristic with the specified handle.
     */
    CharacteristicProperty GetCharacteristicProperties(uint16_t handle);

    /**
     * @brief Get the permissions for the characteristic with the specified handle.
     */
    CharacteristicPermission GetCharacteristicPermissions(uint16_t handle);

    /**
     * @brief Process the JSON defintion of the services, characteristics and descriptors and build
     *        a graph representation of the BLE functionality.
     */
    void BuildGraphFromJson(char *cfg_json);
};

#endif