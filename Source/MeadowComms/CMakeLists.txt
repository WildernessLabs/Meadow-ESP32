# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if (DEBUG EQUAL 1)
    add_compile_definitions(DEBUG)
endif()

if (WIFI_DEBUG EQUAL 1)
    add_compile_definitions(WIFI_DEBUG)
endif()

if (SPI_DEBUG EQUAL 1)
    add_compile_definitions(SPI_DEBUG)
endif()

if (SYSTEM_DEBUG EQUAL 1)
    add_compile_definitions(SYSTEM_DEBUG)
endif()

if (BT_DEBUG EQUAL 1)
    add_compile_definitions(BT_DEBUG)
endif()

if (THREAD_DEBUG EQUAL 1)
    add_compile_definitions(THREAD_DEBUG)
endif()

if (PERFORMANCE_LOGGING EQUAL 1)
    add_compile_definitions(PERFORMANCE_LOGGING)
endif()

if (HEAP_SIZE GREATER 0)
    add_compile_definitions(HEAP_SIZE=${HEAP_SIZE})
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project("MeadowComms")