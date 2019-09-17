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
#include "Secrets.hpp"

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
    //  Temporary code to set up tests.
    //
    Message *connectMessage = (Message *) pvPortMalloc(sizeof(Message));
    connectMessage->MessageType = MessageTypes::Data;
    connectMessage->Interface = Esp32Interfaces::WiFi;
    connectMessage->Function = WiFiFunction::Start;
    connectMessage->MessageID = messageDispatcher->GetNextMessageID();

    Esp32Messaging::WiFiCredentials wifiCredentials;
    wifiCredentials.NetworkName = (char *) NETWORK_NAME;
    wifiCredentials.Password = (char *) WIFI_PASSWORD;
    int bufferLength = Encoders::EncodedWiFiCredentialsBufferSize(&wifiCredentials);
    uint8_t *encodedCredentials = (uint8_t *) pvPortMalloc(bufferLength);
    Encoders::EncodeWiFiCredentials(&wifiCredentials, encodedCredentials);

    connectMessage->Payload = encodedCredentials;
    connectMessage->PayloadLength = bufferLength;
    messageDispatcher->QueueMessageForEsp32(connectMessage);

    Message *getAccessPointsMessage = (Message *) pvPortMalloc(sizeof(Message));
    getAccessPointsMessage->MessageType = MessageTypes::Data;
    getAccessPointsMessage->Interface = Esp32Interfaces::WiFi;
    getAccessPointsMessage->Function = WiFiFunction::GetAccessPoints;
    getAccessPointsMessage->MessageID = messageDispatcher->GetNextMessageID();
    getAccessPointsMessage->Payload = nullptr;
    getAccessPointsMessage->PayloadLength = 0;
    messageDispatcher->QueueMessageForEsp32(getAccessPointsMessage);

//    Message *httpGetMessage = (Message *) pvPortMalloc(sizeof(Message));
//    httpGetMessage->MessageType = MessageTypes::Data;
//    httpGetMessage->Interface = Esp32Interfaces::WiFi;
//    httpGetMessage->Function = WiFiFunction::HttpGet;
//    httpGetMessage->MessageID = 0x02;
//
//    Esp32Messaging::HttpGetRequest requestData;
//    requestData.PortNumber = 80;
//    requestData.ServerName = "www.google.com";
//    requestData.Uri = "/";
//    bufferLength = Encoders::EncodedHttpGetRequestBufferSize(&requestData);
//    uint8_t *encodedHttpGetRequest = (uint8_t *) pvPortMalloc(bufferLength);
//    Encoders::EncodeHttpGetRequest(&requestData, encodedHttpGetRequest);
//
//    httpGetMessage->Payload = encodedHttpGetRequest;
//    httpGetMessage->PayloadLength = bufferLength;
//    messageDispatcher->QueueMessageForEsp32(httpGetMessage);

    //
    //  Main program loop.
    //
//    bool state = false;
    while (true)
    {
//        spi->WaitForData();
//        gpio_set_level(Gpio::ESP32_READY_PIN, state ? 1 : 0);
//        state = !state;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
