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

#include "BluetoothServiceCollection.hpp"
#include "BluetoothAttribute.hpp"
#include "BluetoothHandleManager.hpp"
#include <vector>
#include <map>

using namespace std;

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

    static void HandleServiceAdded(BluetoothService *);
    static void HandleCharacteristicAdded(BluetoothCharacteristic *);
    static void HandleValueChanged(BluetoothCharacteristic *);
    static void HandleDescriptorAdded(BluetoothDescriptor *);

    /**
     * @brief Number of elements in the attribute table.
     */
    uint16_t _attributeTableLength;

    /**
     * @brief 16-bit assigned number for the company identifier.
     */
    uint16_t _companyIdentifier;

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

    void GenerateAttributeTable();
    void GenerateAdvertisingData();
    void SetHandles(uint16_t *handles, uint16_t count);

    /**
     * @brief Get the number of attributes in the attribute table.
     *
     * @return int Number of handles.
     */
    int GetHandleCount()
    {
        return _attributeTableLength;
    }

    void GetHandles(uint16_t *handles);

    uint16_t GetAttributeTableLength();

    void GetCurrentValueForAttributeHandle(uint16_t handle, uint8_t *pData, uint16_t *pLen);
    void SetCurrentValueForAttributeHandle(uint16_t handle, uint8_t *pData, uint16_t len);

    void BuildGraphFromJson(char *cfg_json);
};

#endif