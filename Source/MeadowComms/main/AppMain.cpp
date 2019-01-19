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

/*
 *      WiFi object.
 */
WiFi *wifi;

/*
 *      UART object.
 */
UART *uart;

/*
 *      Main program loop for the ESP32 as specified by the FreeRTOS documentation.
 */
extern "C" void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    wifi = new(WiFi);
    uart = new (UART);
    SetupMessageQueues();
    uart->Setup();
    wifi->Setup();
    xTaskCreate(BluetoothTask, BLUETOOTH_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
    xTaskCreate(MeshNetworkTask, MESH_NETWORK_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);

    esp_err_t result = nvs_flash_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
}
