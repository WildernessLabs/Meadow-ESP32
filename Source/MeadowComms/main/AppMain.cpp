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

void TestBluetoothMessage(MessageDispatcher *dispatcher);

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
// cppcheck-suppress unusedFunction symbolName=app_main
extern "C" void app_main()
{
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

    /*
     *  Set up the messaging system to use SPI hardware communications.
     */
    SpiTransportProvider *spi = new SpiTransportProvider();
    MessageDispatcher * __attribute__((unused)) messageDispatcher = new MessageDispatcher(spi);

    SystemRequestHandler::SendConfigurationAsInterruptMessage();

    TestBluetoothMessage(messageDispatcher);

//     Message message = {};
//     message.MessageType = MessageTypes::Header;
//     message.Interface = Esp32Interfaces::WiFi;
//     message.Function = WiFiFunction::Ioctl;
//     Esp32Messaging::IoctlRequest request;

// #include "WiFiRequestHandler.hpp"

//     request.Command = SIOCGIFFLAGS;
//     message.PayloadLength = Encoders::EncodedIoctlRequestBufferSize(&request);
//     message.Payload = static_cast<uint8_t *>(pvPortMalloc(message.PayloadLength));
//     Encoders::EncodeIoctlRequest(&request, message.Payload);

//     messageDispatcher->QueueMessageForEsp32(&message);

    //
    //  Main program loop.
    //
    while (true)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void TestBluetoothMessage(MessageDispatcher *dispatcher)
{
    // -- start the stack
    Esp32Messaging::BTStackConfig stackConfig;
    stackConfig.DeviceName = (char*)"MEADOW TEST";
    Message startMessage = {};
    startMessage.MessageType = MessageTypes::Header;
    startMessage.Interface = Esp32Interfaces::BlueTooth;
    startMessage.Function = BluetoothFunction::Start;
    startMessage.PayloadLength = Encoders::EncodedBTStackConfigBufferSize(&stackConfig);
    startMessage.Payload = (uint8_t *) pvPortMalloc(startMessage.PayloadLength);
    Encoders::EncodeBTStackConfig(&stackConfig, startMessage.Payload);
    dispatcher->QueueMessageForEsp32(&startMessage);

    // -- create a service
    static const uint8_t generic_gap_uuid[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //this is "generic GAP service" as defined by the BLE SIG
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00
    };

    // build the message payload
    Esp32Messaging::BTServiceDescription serviceDescription;
    serviceDescription.ServiceUUID = 0x00EE; // just a test UUID

    // build the envelope that carries said payload
    Message message = {};
    startMessage.MessageType = MessageTypes::Header;
    startMessage.Interface = Esp32Interfaces::BlueTooth;
    startMessage.Function = BluetoothFunction::AddService;
    message.PayloadLength = Encoders::EncodedBTServiceDescriptionBufferSize(&serviceDescription);
    message.Payload = (uint8_t *) pvPortMalloc(message.PayloadLength);
    Encoders::EncodeBTServiceDescription(&serviceDescription, message.Payload);

    // queue it - this is synchronous as we want to send a handle back to the caller
    dispatcher->QueueMessageForEsp32(&message);


    
    // add a characteristic

    // add another characteristic

    // set the characteristic value

    // read the characteristic value

    static const uint8_t appearance_characteristic_uuid[16] = {
    //00002a01-0000-1000-8000-00805f9b34fb
    //this is "generic GAP service" as defined by the BLE SIG
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0x2a, 0x00, 0x00
    };



    ESP_LOGI(__func__, "Message B");

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

    dispatcher->QueueMessageForEsp32(&message2);

    // now send a message that describes an attribute to add
}
