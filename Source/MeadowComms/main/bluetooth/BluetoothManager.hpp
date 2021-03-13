#ifndef _BLUETOOTH_MANAGER_HPP_
#define _BLUETOOTH_MANAGER_HPP_

#include "BluetoothServiceCollection.hpp"
#include "BluetoothAttribute.hpp"
#include "BluetoothHandleManager.hpp"
#include <vector>

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"


using namespace std;

class BluetoothManager 
{
    private:
        BluetoothHandleManager _handleManager;

        static void HandleServiceAdded(BluetoothService *);
        static void HandleCharacteristicAdded(BluetoothCharacteristic *);
        static void HandleValueChanged(BluetoothCharacteristic *);
        static void HandleDescriptorAdded(BluetoothDescriptor *);

        esp_gatts_attr_db_t* _attribute_table;
        uint16_t _attributeTableLength;
        esp_ble_adv_data_t _advertisingData;
        esp_ble_adv_params_t _advertisingParameters;
        esp_ble_adv_data_t _scanResponseData;
        
    public:
        BluetoothManager();
        BluetoothServiceCollection Services;


        void GenerateAttributeTable();
        void GenerateAdvertisingData();
        void SetHandles(uint16_t* handles, uint16_t count);

        esp_gatts_attr_db_t* GetAttributeTable();
        esp_ble_adv_data_t *GetAdvertisingData();
        esp_ble_adv_data_t *GetScanResponseData();
        esp_ble_adv_params_t *GetAdvertisingParameters();
        uint16_t GetAttributeTableLength();

        // TODO: add high-level graph handler so consumer only uses this instance to manage all BT stuff
};

#endif