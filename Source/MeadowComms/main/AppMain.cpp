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

#include "Tasks.hpp"
#include "UARTTask.h"
#include "WiFiTask.h"
#include "Messaging.h"
#include "Encoders.h"
#include "WiFi.hpp"

WiFi *wifi;

extern "C" void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    wifi = new(WiFi);
    SetupMessageQueues();
    SetupUART();
    wifi->SetupWiFi();
    xTaskCreate(UARTRxTask, "UART_RX_TASK", 1024 * 2, NULL, configMAX_PRIORITIES - 5, &xUARTReceiveTaskHandle);
    xTaskCreate(UARTTxTask, "UART_TX_TASK", 1024 * 2, NULL, configMAX_PRIORITIES - 5, &xUARTTransmitTaskHandle);
    xTaskCreate(BluetoothTask, BLUETOOTH_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
    xTaskCreate(MeshNetworkTask, MESH_NETWORK_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);

    esp_err_t result = nvs_flash_init();
//    system_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    //Start();
//    GetAvailableNetworks();

    
/**
    uint32_t messageNumber = 0;
    const TickType_t xDelay = 1500 / portTICK_PERIOD_MS;
    while (1)
    {
        uint8_t *buffer = pvPortMalloc(APP_RESPONSE_HEADER_LENGTH);
        *buffer = 0;
        EncodeUInt32(messageNumber, buffer + APP_RESPONSE_CODE);
        EncodeUInt32(1, buffer + APP_RESPONSE_MESSAGE_ID);
        EncodeUInt32(0, buffer + APP_RESPONSE_PAYLOAD_LENGTH);
        messageNumber++;
        struct ApplicationResponse response;

        response.interface = 0;
        response.code = 700;
        response.messageID = messageNumber;
        response.payload = buffer;
        response.payloadLength = APP_RESPONSE_PAYLOAD_LENGTH;

        vTaskDelay(xDelay);
        if (xQueueSendToBack(UARTTransmitQueueHandle, &response, 10 / portTICK_PERIOD_MS) != pdPASS)
        {
            ESP_LOGI(__func__, "Error sending response, cannot queue message");
        }
        vTaskDelay(xDelay);
    }
**/

}
