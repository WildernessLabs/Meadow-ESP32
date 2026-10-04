idf_component_register(SRCS "unit_tests.c"
                    INCLUDE_DIRS "."
                                "../../build/config"
                                # "../../components/F7Services"
                                # "../../components/F7Services/WiFi"
                                # "../../components/F7Services/System"
                                # "../../components/F7Services/Messaging"
                                # "../../components/F7Services/Exceptions"
                                # "../../components/F7Services/Bluetooth"
                                # "../../components/F7Services/Diagnostics"
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/WarningsAsErrors.cmake)
meadow_warnings_as_errors(${COMPONENT_LIB})
