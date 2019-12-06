/*
 *      Main application for the ESP32 code for Meadow.
 */
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_ota_ops.h"

#include "Logging.hpp"
#include "MessageDispatcher.hpp"
#include "SpiTransportProvider.hpp"
#include "SharedEnums.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
// cppcheck-suppress unusedFunction symbolName=app_main
extern "C" void app_main()
{
    Logging::Setup();
    esp_log_level_set("*", ESP_LOG_INFO);

    const esp_app_desc_t *applicationDescription = esp_ota_get_app_description();
    ESP_LOGI(__func__, "\n\n    ********** Starting Application (%s version %s) **********\n", applicationDescription->project_name, applicationDescription->version);

    /*
     *  Set up the messaging system to use SPI hardware communications.
     */
    SpiTransportProvider *spi = new SpiTransportProvider();
    MessageDispatcher * __attribute__((unused)) messageDispatcher = new MessageDispatcher(spi);

    esp_err_t result = nvs_flash_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);

    //
    //  Main program loop.
    //
    while (true)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
