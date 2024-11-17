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

#include "sdkconfig.h"

#include "BluetoothServiceCollection.hpp"
#include "BluetoothAttribute.hpp"
#include "BluetoothHandleManager.hpp"
#include <vector>
#include <map>

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

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
    struct length_val
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
     * @brief Handle manager for the BluetoothManager.
     */
    BluetoothHandleManager _handleManager;

    /**
     * @brief Map of the values for the BluetoothManager along with their IDs.
     */
    std::map<uint16_t, length_val> _valueMap;

    static void HandleServiceAdded(BluetoothService *);
    static void HandleCharacteristicAdded(BluetoothCharacteristic *);
    static void HandleValueChanged(BluetoothCharacteristic *);
    static void HandleDescriptorAdded(BluetoothDescriptor *);

    /**
     * @brief Table of attributes in the GATT database.
     */
    esp_gatts_attr_db_t *_attribute_table;

    /**
     * @brief Number of elements in the attribute table.
     */
    uint16_t _attributeTableLength;

    /**
     * @brief Advertising data content, according to "Supplement to the Bluetooth Core Specification".
     */
    esp_ble_adv_data_t _advertisingData;

    /**
     * @brief Advertising parameters.
     */
    esp_ble_adv_params_t _advertisingParameters;

    /**
     * @brief Advertising response data.
     */
    esp_ble_adv_data_t _scanResponseData;

public:
    /**
     * @brief Name of the device.
     */
    char DeviceName[256];

    BluetoothManager();

    /**
     * @brief Collection of services for this device.
     */
    BluetoothServiceCollection Services;

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

    esp_gatts_attr_db_t *GetAttributeTable();
    esp_ble_adv_data_t *GetAdvertisingData();
    esp_ble_adv_data_t *GetScanResponseData();
    esp_ble_adv_params_t *GetAdvertisingParameters();
    uint16_t GetAttributeTableLength();

    void GetCurrentValueForAttributeHandle(uint16_t handle, uint8_t *pData, uint16_t *pLen);
    void SetCurrentValueForAttributeHandle(uint16_t handle, uint8_t *pData, uint16_t len);

    void BuildGraphFromJson(char *cfg_json);
};

#endif