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
                                "ProgressiveDelay.cpp"
                                "Utils.cpp"

                                "MeshNetwork/MeshNetworkRequestHandler.cpp"

                                "Messaging/Encoders.cpp"
                                "Messaging/Esp32Messaging.cpp"
                                "Messaging/MessageDispatcher.cpp"
                                "Messaging/RequestHandlerBase.cpp"
                                "Messaging/SpiTransportProvider.cpp"
                                "Messaging/TransportProviderBase.cpp"

                                "Exceptions/ThreadPoolException.cpp"

                                "Bluetooth/BluetoothRequestHandler.cpp"
                                "Bluetooth/BluetoothAttribute.cpp"
                                "Bluetooth/BluetoothCharacteristic.cpp"
                                "Bluetooth/BluetoothCharacteristicValue.cpp"
                                "Bluetooth/BluetoothHandleManager.cpp"
                                "Bluetooth/BluetoothService.cpp"
                                "Bluetooth/BluetoothServiceCollection.cpp"
                                "Bluetooth/Uuid.cpp"
                                "Bluetooth/BluetoothManager.cpp"

                                "Diagnostics/Diagnostics.cpp"

                        INCLUDE_DIRS "."
                                "../../main"
                                "../../build/config"
                                "WiFi"
                                "System"
                                "Messaging"
                                "MeshNetwork"
                                "Bluetooth"
                                "Exceptions"
                                "../rapidjson"

                        REQUIRES app_trace app_update bootloader bootloader_support bt console 
                                cxx driver efuse esp-tls esp_common esp_eth esp_event 
                                esp_gdbstub esp_hid esp_http_client esp_http_server esp_https_ota 
                                esp_https_server esp_local_ctrl esp_netif esp_ringbuf 
                                esp_rom esp_system esp_timer
                                esp_wifi espcoredump esptool_py fatfs freertos heap idf_test 
                                json log lwip main mbedtls mqtt newlib nvs_flash 
                                partition_table perfmon protobuf-c protocomm pthread sdmmc soc spi_flash 
                                spiffs tcp_transport ulp unity vfs wear_levelling 
                                wifi_provisioning wpa_supplicant xtensa
                        )
