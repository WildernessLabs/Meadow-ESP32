//
//  Implementation of the UART code for Meadow.
//
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include "stdlib.h"

#include "UARTTask.h"
#include "Messaging.h"
#include "WiFiTask.h"
#include "BluetoothTask.h"
#include "MeshNetworkTask.h"

/// <summary>
///     Handle for the UART receive task.
/// </summary>
TaskHandle_t xUARTReceiveTaskHandle = NULL;

/// <summary>
///     Handle for the UART transmit task.
/// </summary>
TaskHandle_t xUARTTransmitTaskHandle = NULL;

/// <summary>
///     Name of the UART receive task (used for logging).
/// </summary>
char *UART_RX_TASK_NAME = "UART_RX_TASK";

/// <summary>
///     Name of the UART transmit task (used for logging).
/// </summary>
char *UART_TX_TASK_NAME = "UART_TX_TASK";

/// <summary>
///     Configure the UART and install the driver.
///
///     UART should be configured as 115200 baud, no partity, one stop bit, 8 data bits.
/// </summary>
void SetupUART()
{
    const uart_config_t uart_config =
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

/// <summary>
///     UART transmit task.
/// </summary>
void UARTTxTask(void *pvParameters)
{
    esp_log_level_set(UART_TX_TASK_NAME, ESP_LOG_INFO);
    while (true)
    {
        char *message = NULL;
        BaseType_t result = xQueueReceive(UARTTransmitQueueHandle, &message, portMAX_DELAY);
        if (result == pdTRUE)
        {
            const int len = strlen(message);
            const int txBytes = uart_write_bytes(UART_NUM_1, message, len);
            ESP_LOGI(UART_TX_TASK_NAME, "Wrote %d bytes to UART", txBytes);
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    vTaskDelete(xUARTTransmitTaskHandle);
}

/// <summary>
///     UART receive task
/// </summary>
void UARTRxTask(void *pvParameters)
{
    esp_log_level_set(UART_RX_TASK_NAME, ESP_LOG_INFO);
    uint8_t* data = (uint8_t *) malloc(RX_BUF_SIZE + 1);
    while (true)
    {
        int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0)
        {
            ESP_LOGI(UART_RX_TASK_NAME, "Read %d bytes.", rxBytes);
            ESP_LOG_BUFFER_HEXDUMP(UART_RX_TASK_NAME, data, rxBytes, ESP_LOG_INFO);
            if ((data[TM_HEADER_BYTE0] == 0xaa) && (data[TM_HEADER_BYTE1] == 0xaa))
            {
                //
                //  The data packets from the sended will have an embedded CRC in the header.  The
                //  original CRC will have been calculated with the CRC byte in the header set to 0
                //  so extract the CRC value but then set the CRC in the header to 0 so that we can
                //  check the CRC.
                //
                uint8_t crc = data[TM_CRC];
                data[TM_CRC] = 0;
                if (crc != CRC8(data, rxBytes))
                {
                    ESP_LOGI(UART_RX_TASK_NAME, "CRC mismatch %d sent.", crc);
                }
                uint16_t payloadLength = (data[TM_LENGTH_BYTE0] | (data[TM_LENGTH_BYTE1] << 8));
                if (rxBytes == (payloadLength + TM_PAYLOAD_START))
                {
                    struct TaskMessage message;
                    message.interface = data[TM_INTERFACE];
                    message.function = data[TM_FUNCTION_BYTE0];
                    message.function |= (data[TM_FUNCTION_BYTE1] << 8);
                    message.function |= (data[TM_FUNCTION_BYTE2] << 16);
                    message.function |= (data[TM_FUNCTION_BYTE3] << 24);
                    message.payloadLength = payloadLength;
                    if (payloadLength > 0)
                    {
                        message.payload = pvPortMalloc(payloadLength);
                        memcpy(message.payload, data + TM_PAYLOAD_START, payloadLength);
                    }
                    else
                    {
                        message.payload = NULL;
                    }
                    char *taskName = NULL;
                    QueueHandle_t queueHandle = NULL;
                    switch (message.interface)
                    {
                        case FUNC_WIFI:
                            taskName = WIFI_TASK_NAME;
                            queueHandle = WiFiQueueHandle;
                            break;
                        case FUNC_BLUETOOTH:
                            taskName = BLUETOOTH_TASK_NAME;
                            queueHandle = BluetoothQueueHandle;
                            break;
                        case FUNC_MESH_NETWORK:
                            taskName = MESH_NETWORK_TASK_NAME;
                            queueHandle = MeshNetworkQueueHandle;
                            break;
                    }
                    if (message.interface != FUNC_SYSTEM)
                    {
                        if (xQueueSendToBack(queueHandle, &message, 0) == pdTRUE)
                        {
                            ESP_LOGI(taskName, "Message queued.");
                        }
                        else
                        {
                            ESP_LOGI(taskName, "Message cannot be queued.");
                            if (message.payload != NULL)
                            {
                                vPortFree(message.payload);
                            }
                        }
                    }
                    else
                    {
                        ESP_LOGI("SYSTEM", "System tasks not implemented.");
                    }
                }
            }
            else
            {
                ESP_LOGI(UART_RX_TASK_NAME, "Header bytes not detected");
            }
        }
    }
    //
    //  We should only get here if the code in the while loop generates a fault.
    //  The documentation for FreeRTOS states that we should delete the task
    //  should we get here.
    //
    free(data);
    vTaskDelete(xUARTReceiveTaskHandle);
}