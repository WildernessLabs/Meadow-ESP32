/*
 *      Main application for the ESP32 code for Meadow.
 */
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_ota_ops.h"

#include "NvsManager.hpp"
#include "Logging.hpp"
#include "MessageDispatcher.hpp"
#include "SpiTransportProvider.hpp"

#include "GenericList.hpp"
#include "NvsManager.hpp"

#include "WiFiRequestHandler.hpp"
#include "SpiTransportProvider.hpp"

#if defined(CONFIG_HEAP_TRACING_STANDALONE) || defined(CONFIG_HEAP_TRACING_TOHOST)

#include "esp_heap_trace.h"

#endif

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

#define NUM_RECORDS 400
static heap_trace_record_t trace_records[NUM_RECORDS];

#endif

#define APP_MAIN_COMPONENT_NAME "AppMain"
#define BT_FULL_LOGGING
#define BT_DEBUG

void SendTestMessage(MessageDispatcher *dispatcher);

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
// cppcheck-suppress unusedFunction symbolName=app_main
extern "C" void app_main()
{
    esp_log_level_set("*", ESP_LOG_NONE);

#if defined(DEBUG) || defined(WIFI_DEBUG) || defined(SPI_DEBUG) || defined(THREAD_DEBUG) || defined(SYSTEM_DEBUG) || defined(BT_DEBUG)
    #warning "Debug is enabled."
    esp_log_level_set(APP_MAIN_COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(WIFI_DEBUG)
    #warning "WIFI debugging is enabled."
    esp_log_level_set(WiFiRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(SPI_DEBUG)
    #warning "SPI debugging is enabled."
    esp_log_level_set(SpiTransportProvider::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(THREAD_DEBUG)
    #warning "ThreadPool debugging is enabled."
    esp_log_level_set(Thread::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(BT_DEBUG)
    #warning "Bluetooth debugging is enabled."
    esp_log_level_set(BluetoothRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#if defined(SYSTEM_DEBUG)
    #warning "System debugging is enabled."
    esp_log_level_set(SystemRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
#endif

    esp_log_level_set(Logging::COMPONENT_NAME, ESP_LOG_INFO);
    Logging::Setup();
    NvsManager::Setup();

#ifdef CONFIG_HEAP_TRACING_TOHOST
    #warning "Heap tracing to host turned on."

    ESP_ERROR_CHECK(heap_trace_init_tohost());
    heap_trace_start(HEAP_TRACE_ALL);
#endif

#ifdef CONFIG_HEAP_TRACING_STANDALONE
    #warning "Standalone heap tracing turned on."

    ESP_ERROR_CHECK(heap_trace_init_standalone(trace_records, NUM_RECORDS));
    ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
#endif

    const esp_app_desc_t *applicationDescription = esp_ota_get_app_description();
    printf("********** Starting Application (%s version %s) **********\n", applicationDescription->project_name, applicationDescription->version);
ESP_LOGI(__func__, "CP-1");
    ESP_LOGI(APP_MAIN_COMPONENT_NAME, "\n\n     ********** Starting Application (%s version %s) **********\n", applicationDescription->project_name, applicationDescription->version);

    /*
     *  Set up the messaging system to use SPI hardware communications.
     */
    SpiTransportProvider *spi = new SpiTransportProvider();
    MessageDispatcher * __attribute__((unused)) messageDispatcher = new MessageDispatcher(spi);

    SendTestMessage(messageDispatcher);

    //
    //  Main program loop.
    //
    while (true)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}


void SendTestMessage(MessageDispatcher *messageDispatcher)
{
    /*
    ESP_LOGI(APP_MAIN_COMPONENT_NAME, "Message A");

    // fire up the BT stack
    Esp32Messaging::BTServicesDescription desc;
    desc.DeviceName = (char*)"Meadow ABCD";

    Message message = {};
    message.MessageType = MessageTypes::Header;
    message.Interface = Esp32Interfaces::BlueTooth;
    message.Function = BluetoothFunction::Start;
    
    message.PayloadLength = Encoders::EncodedBTServicesDescriptionBufferSize(&desc);
    message.Payload = (uint8_t *) pvPortMalloc(message.PayloadLength);
    Encoders::EncodeBTServicesDescription(&desc, message.Payload);

    messageDispatcher->QueueMessageForEsp32(&message);
    */
   
    static const uint8_t generic_gap_uuid[16] = {
    // LSB <--------------------------------------------------------------------------------> MSB 
    //this is "generic GAP service" as defined by the BLE SIG
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00
    };
    static const uint8_t appearance_characteristic_uuid[16] = {
    //00002a01-0000-1000-8000-00805f9b34fb
    //this is "generic GAP service" as defined by the BLE SIG
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0x2a, 0x00, 0x00
    };



    ESP_LOGI(APP_MAIN_COMPONENT_NAME, "Message B");

    // we'll add a characteristic.  They must be part of a Service, so that will get created underneath as well
    Esp32Messaging::BTCharacteristicDesc cdesc;
    memcpy(cdesc.ServiceUUID, generic_gap_uuid, sizeof(generic_gap_uuid));
    cdesc.ServiceDescription = (char*)"Meadow Generic Service";
    memcpy(cdesc.CharacteristicUUID, appearance_characteristic_uuid, sizeof(appearance_characteristic_uuid));
    cdesc.CharacteristicProperties = ESP_GATT_CHAR_PROP_BIT_WRITE|ESP_GATT_CHAR_PROP_BIT_READ;
    
    Message message2 = {};
    message2.MessageType = MessageTypes::Header;
    message2.Interface = Esp32Interfaces::BlueTooth;
    message2.Function = BluetoothFunction::AddCharacteristic;
    
    message2.PayloadLength = Encoders::EncodedBTCharacteristicDescBufferSize(&cdesc);
    message2.Payload = (uint8_t *) pvPortMalloc(message2.PayloadLength);
    Encoders::EncodeBTCharacteristicDesc(&cdesc, message2.Payload);

    ESP_LOGI(APP_MAIN_COMPONENT_NAME, "QueueMessageForEsp32");
    messageDispatcher->QueueMessageForEsp32(&message2);

    // now send a message that describes an attribute to add


// #include "WiFiRequestHandler.hpp"

//    Esp32Messaging::IoctlRequest request;
//     request.Command = SIOCGIFFLAGS;
//     message.PayloadLength = Encoders::EncodedIoctlRequestBufferSize(&request);
//     message.Payload = static_cast<uint8_t *>(pvPortMalloc(message.PayloadLength));
//     Encoders::EncodeIoctlRequest(&request, message.Payload);


    ESP_LOGI(APP_MAIN_COMPONENT_NAME, "Exit");
}
