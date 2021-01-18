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

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
// cppcheck-suppress unusedFunction symbolName=app_main
extern "C" void app_main()
{
    esp_log_level_set("*", ESP_LOG_NONE);

#if defined(DEBUG) || defined(WIFI_DEBUG) || defined(SPI_DEBUG) || defined(THREAD_DEBUG) || defined(SYSTEM_DEBUG)
    #warning "Debug is enabled."
    esp_log_level_set(APP_MAIN_COMPONENT_NAME, ESP_LOG_INFO);
#endif

#ifdef WIFI_DEBUG
    #warning "WIFI debugging is enabled."
    esp_log_level_set(WiFiRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#ifdef SPI_DEBUG
    #warning "SPI debugging is enabled."
    esp_log_level_set(SpiTransportProvider::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#ifdef THREAD_DEBUG
    #warning "ThreadPool debugging is enabled."
    esp_log_level_set(Thread::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#ifdef BT_DEBUG
    #warning "Bluetooth debugging is enabled."
    esp_log_level_set(Thread::COMPONENT_NAME, ESP_LOG_INFO);
#endif

#ifdef SYSTEM_DEBUG
    #warning "System debugging is enabled."
    esp_log_level_set(SystemRequestHandler::COMPONENT_NAME, ESP_LOG_INFO);
#endif

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
    ESP_LOGI(APP_MAIN_COMPONENT_NAME, "\n\n    ********** Starting Application (%s version %s) **********\n", applicationDescription->project_name, applicationDescription->version);

    /*
     *  Set up the messaging system to use SPI hardware communications.
     */
    SpiTransportProvider *spi = new SpiTransportProvider();
    MessageDispatcher * __attribute__((unused)) messageDispatcher = new MessageDispatcher(spi);

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
