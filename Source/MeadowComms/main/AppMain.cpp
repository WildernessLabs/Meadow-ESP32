/*
 *      Main application for the ESP32 code for Meadow.
 */

#include "MeshNetworkRequestHandler.hpp"
#include "WiFiRequestHandler.hpp"
#include "BluetoothRequestHandler.hpp"
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
#include "Messaging.h"
#include "UART.hpp"
#include "Logging.hpp"
#include "SharedEnums.hpp"
#include "SystemRequestHandler.hpp"

/*
 *      WiFi object.
 */
WiFiRequestHandler *wifi = NULL;

/*
 *      UART object.
 */
UART *uart = NULL;

/*
 *      Bluetooth object.
 */
BluetoothRequestHandler *bluetooth = NULL;

/*
 *      Mesh networking object.
 */
MeshNetworkRequestHandler *meshNetwork = NULL;

/*
 *      System request handler.
 */
SystemRequestHandler *systemRequestHandler = NULL;


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
    wifi = new(WiFiRequestHandler);
    uart = new (UART);

    uart->Setup();
    wifi->Setup();

    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
}
