/*
 *      Main application for the ESP32 code for Meadow.
 */
#include "sdkconfig.h"

#include <string.h>
#include <ctime>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "version.h"

#include "AppMain.hpp"
#include "Encoders.hpp"
#include "NvsManager.hpp"
#include "Logging.hpp"
#include "MessageDispatcher.hpp"
#include "SpiTransportProvider.hpp"
#include "SystemRequestHandler.hpp"

/**
 * @brief global variable holding the system build information.
 */
meadow_version_t g_application_version = {
    .major = VERSION_MAJOR,
    .minor = VERSION_MINOR,
    .revision = VERSION_REVISION,
    .build = VERSION_BUILD,
    .day = VERSION_BUILD_DAY,
    .month = VERSION_BUILD_MONTH,
    .year = VERSION_BUILD_YEAR,
    .hour = VERSION_BUILD_HOUR,
    .minute = VERSION_BUILD_MINUTE,
    .second = VERSION_BUILD_SECOND,
    .hash = VERSION_BUILD_HASH_NUMBER,
    .branch_name = (char *) VERSION_GIT_REF};

//
//  Checks to see if TRACE or DEBUG is turned on and issues a warning if they are.
//
//  These checks are included in here so that they are processed only once.
//
#if defined(TRACE_MESSAGES)

    #pragma message "Trace message output is enabled - turn this feature off for release builds."

#endif

#if defined(DEBUG) || defined(WIFI_DEBUG) || defined(SPI_DEBUG) || defined(THREAD_DEBUG) || defined(SYSTEM_DEBUG) || defined(BLUETOOTH_DEBUG) || defined(MESSAGES_DEBUG)
    #pragma message "Debug is enabled."
#endif

//
//  This definition is required for the trace message macros.
//  See Logging.hpp for more information on the trace message macros
//
#define COMPONENT_NAME APP_MAIN_COMPONENT_NAME

#if !defined(CONFIG_WIFI_SSID)
    #define CONFIG_WIFI_SSID "SSID"
#endif

#if !defined(CONFIG_WIFI_PASSWORD)
    #define CONFIG_WIFI_PASSWORD "PASSWORD"
#endif

/**
 * @brief Message dispatcher for the system.
 */
MessageDispatcher *_messageDispatcher = nullptr;

/**
 *  @brief Callback for the heap allocation failures.
 *
 *  Reports the allocation that failed through the logging system rather than the
 *  raw UART and let the caller's existing EspOutOfMemory handling run.
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
    ESP_LOGE(COMPONENT_NAME, "%s failed to allocate %" PRIu32 " bytes with 0x%" PRIx32 " capabilities.", functionName, static_cast<uint32_t>(requestedSize), caps);
}

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
// cppcheck-suppress unusedFunction symbolName=app_main
extern "C" void app_main()
{
#if defined(DEBUG)
    esp_err_t error = heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook);
    ESP_ERROR_CHECK(error);
#endif

    Logging::Setup();

    struct tm t;
    memset(&t, 0, sizeof(struct tm));
    t.tm_mon = g_application_version.month - 1;
    char month_text[4];
    strftime(month_text, 4, "%b", &t);

    ESP_LOGI(
        COMPONENT_NAME, "Version: %" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ", built %02" PRIu8 " %s 20%02" PRIu8 " %02" PRIu8 ":%02" PRIu8 ":%02" PRIu8 " UTC (%08" PRIu32 ":%s)", g_application_version.major, g_application_version.minor, g_application_version.revision,
        g_application_version.build, g_application_version.day, month_text, g_application_version.year, g_application_version.hour, g_application_version.minute, g_application_version.second, g_application_version.hash, g_application_version.branch_name);

    NvsManager::Setup();

    /*
     *  Set up the messaging system to use SPI hardware communications.
     */
    SpiTransportProvider *spi = new SpiTransportProvider();
    _messageDispatcher = new MessageDispatcher(spi);

    //
    //  The following shows how we can reduce the amount of heap memory the system has available.
    //  The two print statements allow us to see how much memory is available before and after
    //  the allocation to verify that the allocation has worked and also to see how much memory
    //  is available.
    //
    //  The value of 50,000 bytes was determined by empirical means.  It allows for an application
    //  to start the network driver, connect to an access point and make a request to a web server.
    //  The value should be changed depending upon the actual test being performed.
    //
    // heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    // char *dummy = static_cast<char *>(pvPortMalloc(50000));
    // if (dummy != NULL)
    // {
    //     dummy[0] = 0;
    // }
    // else
    // {
    //     ESP_LOGE(COMPONENT_NAME, "Failed to allocate dummy buffer.");
    // }
    // heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

    SystemRequestHandler::SendConfigurationAsInterruptMessage();

    //
    //  We suspend this task as the message processing is performed by the objects created above.
    //
    vTaskSuspend(NULL);
}
