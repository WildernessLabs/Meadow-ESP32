#include "BluetoothManager.hpp"

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

BluetoothManager::BluetoothManager()
    : Services(&this->_handleManager)
{
    Services.OnServiceAdded = &BluetoothManager::HandleServiceAdded;
    Services.OnCharacteristicAdded = &BluetoothManager::HandleCharacteristicAdded;
    Services.OnValueChanged = &BluetoothManager::HandleValueChanged;
}

void BluetoothManager::HandleServiceAdded(BluetoothService *service)
{
    printf("New Service: 0x%04X (%s)\n", service->Handle,  service->Name.c_str());
}

void BluetoothManager::HandleCharacteristicAdded(BluetoothCharacteristic *characteristic)
{
    printf("New Characteristic: declaration @ 0x%04X value @ 0x%04X\n", characteristic->Declaration->Handle,  characteristic->Value->Handle);
}

void BluetoothManager::HandleValueChanged(BluetoothCharacteristic *characteristic)
{
    printf("New Characteristic Value: value @ 0x%04X is %i bytes\n", characteristic->Value->Handle, characteristic->Value->CurrentLength);
}

void BluetoothManager::HandleDescriptorAdded(BluetoothDescriptor *)
{
    
}

void BluetoothManager::GenerateAttributeTable()
{
    // TODO: how many attributes are we going to need?
    // 1 for each service
    // 2 for each characteristic (declaration + value)
    // 1 for each descriptor

    auto count = 0;

    // not thrilled with the double-iteration but:
    // - the list is small
    // - it's better than dynamic resizing the array for every attribute we need to add
    for (const auto& svc : Services.GetServiceDefinitions())
    {
        // 1 for the service
        count++;

        for (const auto& characteristic : svc->GetCharacteristicDefinitions())
        {
            count += 2; // +2 for the declaration and value

            for (const auto& descriptor : characteristic->GetDescriptorDefinitions())
            {
                count ++; // +1 for the decriptor
            }
        }
    }

    _attribute_table = new esp_gatts_attr_db_t[count];

    auto index = 0;
    for (const auto& svc : Services.GetServiceDefinitions())
    {
        // service attribute - all services are primary for now        
        _attribute_table[index++] =
            {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
            sizeof(uint16_t), sizeof(svc->Uuid), (uint8_t *)&svc->Uuid}};

        for (const auto& characteristic : svc->GetCharacteristicDefinitions())
        {
            // declaration attribute
            /*
                [1]            =
                {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic_declaration_uuid, ESP_GATT_PERM_READ,
                CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify}},
            */            
            _attribute_table[index++] =
                {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic->Declaration->Uuid, (uint16_t)characteristic->Declaration->Permission,
                CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&characteristic->Declaration->Properties}};

            // value attribute
            /*
                [2]             =
                {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&heart_rate_meas_uuid, ESP_GATT_PERM_READ,
                HRPS_HT_MEAS_MAX_LEN, 0, NULL}},
            */
            _attribute_table[index++] =
                {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&characteristic->Value->Uuid, (uint16_t)characteristic->Value->Permission,
                characteristic->Value->MaxLength, 0, NULL}}; // <- 0, NULL means "current value length is 0, current value data is null"

            for (const auto& descriptor : characteristic->GetDescriptorDefinitions())
            {
                // descriptor attribute
                /*
                    // Heart Rate Measurement Characteristic - Client Characteristic Configuration Descriptor
                    [3]        =
                    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
                    sizeof(uint16_t),sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},

                */
                _attribute_table[index++] =
                    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&descriptor->Uuid, (uint16_t)descriptor->Permission,
                    descriptor->MaxLength, descriptor->CurrentLength, (uint8_t *)descriptor->Value}};
            }
        }
    }        
}
