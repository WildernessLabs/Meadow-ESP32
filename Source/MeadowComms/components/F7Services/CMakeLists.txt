set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")

idf_component_register( SRCS    "WiFi/WiFiRequestHandler.cpp"
                                "WiFi/AccessPoint.cpp"
                                "WiFi/PosixAPIHandlers.cpp"

                                "System/ThreadPool.cpp"
                                "System/Thread.cpp"
                                "System/NvsManager.cpp"
                                "System/SystemRequestHandler.cpp"
                                "System/FileSystem.cpp"

                                "Mapping.cpp"
                                "Logging.cpp"

                                "MeshNetwork/MeshNetworkRequestHandler.cpp"

                                "Messaging/Encoders.cpp"
                                "Messaging/Esp32Messaging.cpp"
                                "Messaging/MessageDispatcher.cpp"
                                "Messaging/RequestHandlerBase.cpp"
                                "Messaging/SpiTransportProvider.cpp"
                                "Messaging/TransportProviderBase.cpp"

                                "Exceptions/ThreadPoolException.cpp"

                                "Bluetooth/BluetoothRequestHandler.cpp"
                                "Bluetooth/bluetooth/BluetoothCharacteristic.cpp"
                                "Bluetooth/bluetooth/BluetoothCharacteristicValue.cpp"
                                "Bluetooth/bluetooth/BluetoothHandleManager.cpp"
                                "Bluetooth/bluetooth/BluetoothService.cpp"
                                "Bluetooth/bluetooth/BluetoothServiceCollection.cpp"
                                "Bluetooth/bluetooth/Uuid.cpp"
                                "Bluetooth/bluetooth/BluetoothManager.cpp"

                        INCLUDE_DIRS "."
                                "../../main"
                                "../../build/config"
                                "../esp_littlefs/include"
                                "WiFi"
                                "System"
                                "Messaging"
                                "MeshNetwork"
                                "Bluetooth"
                                "Exceptions"
                                "Bluetooth/rapidjson"

                        REQUIRES app_trace app_update bootloader bootloader_support bt console 
                                cxx driver efuse esp-tls esp_adc esp_common esp_eth esp_event 
                                esp_gdbstub esp_hid esp_http_client esp_http_server esp_https_ota 
                                esp_https_server esp_littlefs esp_local_ctrl esp_netif esp_ringbuf 
                                esp_rom esp_system esp_timer
                                esp_wifi espcoredump esptool_py fatfs freertos heap idf_test 
                                json log lwip main mbedtls mqtt newlib nvs_flash 
                                partition_table perfmon protobuf-c protocomm pthread sdmmc soc spi_flash 
                                spiffs tcp_transport ulp unity vfs wear_levelling 
                                wifi_provisioning wpa_supplicant xtensa
                        )
