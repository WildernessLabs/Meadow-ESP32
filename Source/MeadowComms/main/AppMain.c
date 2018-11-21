#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"

#include "UARTTask.h"
#include "WiFiTask.h"
#include "BluetoothTask.h"
#include "MeshNetworkTask.h"
#include "Messaging.h"
#include "Encoders.h"

void app_main()
{
    SetupMessageQueues();
    SetupUART();
    xTaskCreate(UARTRxTask, "UART_RX_TASK", 1024 * 2, NULL, configMAX_PRIORITIES, &xUARTReceiveTaskHandle);
    xTaskCreate(UARTTxTask, "UART_TX_TASK", 1024 * 2, NULL, configMAX_PRIORITIES, &xUARTTransmitTaskHandle);
    xTaskCreate(WiFiTask, WIFI_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
    xTaskCreate(BluetoothTask, BLUETOOTH_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
    xTaskCreate(MeshNetworkTask, MESH_NETWORK_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);

    uint32_t sequenceNumber = 0;
    while (1)
    {
        uint8_t *buffer = pvPortMalloc(APP_RESPONSE_HEADER_LENGTH);
        *buffer = 0;
        EncodeUInt32(sequenceNumber, buffer + APP_RESPONSE_CODE);
        EncodeUInt32(1, buffer + APP_RESPONSE_MESSAGE_ID);
        EncodeUInt32(0, buffer + APP_RESPONSE_PAYLOAD_LENGTH);
        sequenceNumber++;
        struct ApplicationResponse response;

        response.interface = 0;
        response.code = 700;
        response.messageID = 1025;
        response.payload = buffer;
        response.payloadLength = APP_RESPONSE_PAYLOAD_LENGTH;
        if (xQueueSendToBack(UARTTransmitQueueHandle, &response, 0) != pdTRUE)
        {
            ESP_LOGI(__func__, "Error sending response, cannot queue message");
        }
    }
}
