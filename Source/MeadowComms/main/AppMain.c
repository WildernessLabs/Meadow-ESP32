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

void app_main()
{
    SetupMessageQueues();
    SetupUART();
    xTaskCreate(UARTRxTask, UART_RX_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES, &xUARTReceiveTaskHandle);
    xTaskCreate(UARTTxTask, UART_TX_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES, &xUARTTransmitTaskHandle);
    xTaskCreate(WiFiTask, WIFI_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
    xTaskCreate(BluetoothTask, BLUETOOTH_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
    xTaskCreate(MeshNetworkTask, MESH_NETWORK_TASK_NAME, 1024 * 2, NULL, configMAX_PRIORITIES - 1, &xWiFiTaskHandle);
}
