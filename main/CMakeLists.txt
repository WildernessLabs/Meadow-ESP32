idf_component_register(SRCS "AppMain.cpp"
                       INCLUDE_DIRS "."
                                "../build/config"
                                )

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/WarningsAsErrors.cmake)
meadow_warnings_as_errors(${COMPONENT_LIB})
