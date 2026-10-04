idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "."
                    REQUIRES unity F7Services joltwallet__littlefs mbedtls)

include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/WarningsAsErrors.cmake)
meadow_warnings_as_errors(${COMPONENT_LIB})
