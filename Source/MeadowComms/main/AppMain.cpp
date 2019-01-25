/*
 *      Main application for the ESP32 code for Meadow.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "esp_app_trace.h"
#include "string.h"
#include "nvs_flash.h"

#include "Encoders.hpp"
#include "Tasks.hpp"
#include "Messaging.h"
#include "UART.hpp"
#include "WiFi.hpp"
#include "Bluetooth.hpp"
#include "MeshNetwork.hpp"
#include "Logging.hpp"

/*
 *      WiFi object.
 */
WiFi *wifi;

/*
 *      UART object.
 */
UART *uart;

/*
 *      Bluetooth object.
 */
Bluetooth *bluetooth;

/*
 *      Mesh networking object.
 */
MeshNetwork *meshNetwork;

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
extern "C" void app_main()
{
    Logging::Setup();
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_err_t result = nvs_flash_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    wifi = new(WiFi);
    uart = new (UART);
    SetupMessageQueues();

    uart->Setup();
    wifi->Setup();

    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
}
