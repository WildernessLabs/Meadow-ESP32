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

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
extern "C" void app_main()
{
    Logging::Setup();
    esp_log_level_set("*", ESP_LOG_INFO);

    const esp_app_desc_t *applicationDescription = esp_ota_get_app_description();
    ESP_LOGI(__func__, "\n\n    ********** Starting Application (%s version %s) **********\n", applicationDescription->project_name, applicationDescription->version);

    /*
     *      Set up the messaging system to use SPI hardware communications.
     */
    SpiTransportProvider *spi = new SpiTransportProvider();
    MessageDispatcher *messageDispatcher = new MessageDispatcher(spi);

    esp_err_t result = nvs_flash_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);

    bool readyPinState = false;
    gpio_set_level(Gpio::ESP32_READY_PIN, 0);
    while (true)
    {
//        spi->Echo("Hello, world");

//        gpio_set_level(Gpio::ESP32_READY_PIN, readyPinState ? 1 : 0);
        readyPinState = !readyPinState;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
