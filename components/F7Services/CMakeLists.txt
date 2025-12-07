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
                                "Bluetooth"
                                "Exceptions"
                                "Diagnostics"

                        REQUIRES 
                                bt
                                cxx 
                                driver 
                                efuse 
                                fatfs 
                                freertos 
                                heap 
                                log 
                                lwip 
                                nvs_flash
                                partition_table 
                                pthread 
                                spi_flash
                                tcp_transport 
                                ulp 
                                vfs 
                                wpa_supplicant 
                                xtensa
                                esp_netif 
                                esp_system 
                                esp_wifi 
                                espcoredump 
                                esp_common 
                                esp_event
                                json
                                )
