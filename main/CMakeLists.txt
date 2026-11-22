set(MAIN_SRCS "AppMain.cpp")
#
#  The ESP32-S3 build runs in esp-emu, whose virtual Bluetooth controller does not keep
#  esp_bt_controller_get_status up to date; EspEmuBtControllerStatus.c does (see the file).
#
if (IDF_TARGET STREQUAL "esp32s3")
    list(APPEND MAIN_SRCS "EspEmuBtControllerStatus.c")
endif()

idf_component_register(SRCS ${MAIN_SRCS}
                       INCLUDE_DIRS "."
                                "${CMAKE_BINARY_DIR}/config"
                                )

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/WarningsAsErrors.cmake)
meadow_warnings_as_errors(${COMPONENT_LIB})

if (IDF_TARGET STREQUAL "esp32s3")
    foreach(fn esp_bt_controller_init esp_bt_controller_deinit esp_bt_controller_enable
               esp_bt_controller_disable esp_bt_controller_get_status)
        #  -u: libmain.a is searched before the libraries that call these, so pull the object in.
        target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=${fn}" "-u __wrap_${fn}")
    endforeach()
endif()
