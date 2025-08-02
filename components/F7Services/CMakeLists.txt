set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")

file(GLOB_RECURSE F7SERVICES_SOURCES
    ./*.cpp
)

# Remove test files from the sources
list(FILTER F7SERVICES_SOURCES EXCLUDE REGEX ".*/test/.*")


idf_component_register( SRCS    ${F7SERVICES_SOURCES}
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

                        REQUIRES app_trace app_update bootloader bootloader_support bt
                                cxx driver efuse fatfs freertos heap json log lwip main nvs_flash
                                partition_table perfmon protobuf-c protocomm pthread sdmmc soc spi_flash
                                spiffs tcp_transport ulp vfs wear_levelling wpa_supplicant xtensa
                                esp_netif esp_rom esp_system esp_timer esp_wifi espcoredump esp_common esp_event esp_gdbstub esp_hid
                                )
