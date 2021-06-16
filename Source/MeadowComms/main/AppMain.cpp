/*
 *      Main application for the ESP32 code for Meadow.
 */
#include "../build/config/sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"

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

/**
 *  @brief Callback for the heap allocation failures.
 * 
 *  The method prints some information about the method that caused the problem.
 *  It then waits for a while for the message to be output and then generates an
 *  exception to force a core dump (if enabled) and a restart.
 *
 *  @param requestedSize
 *      Amount of space requested.
 * 
 *  @param caps
 *      Capability requested.
 * 
 *  @param functionName
 *      Method that requested the memory.
 */
void heap_caps_alloc_failed_hook(size_t requestedSize, uint32_t caps, const char *functionName)
{
    printf("%s was called but failed to allocate %d bytes with 0x%X capabilities. \n", functionName, requestedSize, caps);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    char *ptr = NULL;
    *ptr = 0;
}

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
// cppcheck-suppress unusedFunction symbolName=app_main
extern "C" void app_main()
{
    esp_err_t error = heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook);
    ESP_ERROR_CHECK(error);

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

//    TestBluetoothMessage(messageDispatcher);

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

#if defined(HEAP_SIZE)

    printf("Constraining heap to %d bytes", HEAP_SIZE * 1024);
    size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint8_t *memory_to_hold = NULL;
    printf("Heap free before initialisation: %d\n", heap_free);
    if (heap_free > HEAP_SIZE)
    {
        size_t amount_to_consume = heap_free - (HEAP_SIZE * 1024);
        if (amount_to_consume > 0)
        {
            printf("Allocating %d bytes\n", amount_to_consume);
            memory_to_hold = (uint8_t *) malloc(amount_to_consume);
            if (memory_to_hold == NULL)
            {
                printf("Memory allocation failed.\n");
            }
        }
    }
    heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    printf("Heap free after initialisation: %d\n", heap_free);

#endif

    //
    //  Main program loop.
    //
    while (true)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

const char* json_test = R"(
{
    "deviceName":"MY MEADOW",
    "services": [
        {
            "name": "Service A",
            "uuid": 254,
            "characteristics": [
                {
                    "uuid": "017e99d6-8a61-11eb-8dcd-0242ac130003",
                    "permission": 17,
                    "props": 10,
                    "len": 4
                },
                {
                    "uuid": 10755,
                    "permission": 2,
                    "props": 16,
                    "len": 4
                }
            ]
        }
    ]
})";

const char* json_test2 = R"(
{
    "deviceName":"MY MEADOW",
    "services": [
        {
            "name": "Service A",
            "uuid": 254,
            "characteristics": [
                {
                    "uuid": "017e99d6-8a61-11eb-8dcd-0242ac130003",
                    "permission": 17,
                    "props": 10,
                    "len": 4,
                    "descriptors": [
                        {
                        "uuid": 10498,
                        "permission": 1,
                        "len": 2,
                        "value": 0
                        }
                    ]
                },
                {
                    "uuid": 10755,
                    "permission": 2,
                    "props": 16,
                    "len": 4
                }
            ]
        }
    ]
})";

void TestBluetoothMessage(MessageDispatcher *dispatcher)
{
    // for now we'll use one message that starts and sets up everything.  
    // We'll need to separate this and actually have data going in, but this is a baby step toward that
    
    Esp32Messaging::BTStackConfig stackConfig;
    stackConfig.Config = (char*)json_test;

    Message startMessage = {};
    startMessage.MessageType = MessageTypes::Header;
    startMessage.Interface = Esp32Interfaces::BlueTooth;
    startMessage.Function = BluetoothFunction::Start;
    startMessage.PayloadLength = Encoders::EncodedBTStackConfigBufferSize(&stackConfig);
    startMessage.Payload = (uint8_t *) pvPortMalloc(startMessage.PayloadLength);
    Encoders::EncodeBTStackConfig(&stackConfig, startMessage.Payload);
    dispatcher->QueueMessageForEsp32(&startMessage);

}
