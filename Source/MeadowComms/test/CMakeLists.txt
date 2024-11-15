# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.5)

# Include the components directory of the main application:
#
set(EXTRA_COMPONENT_DIRS "../components")

set(SDKCONFIG_DEFAULTS "sdkconfig.defaults\;sdkconfig.tests")

file(REMOVE sdkconfig)
set(SDKCONFIG_FILES "sdkconfig.defaults")
list(APPEND SDKCONFIG_FILES "\;sdkconfig.tests")
set(SDKCONFIG_DEFAULTS ${SDKCONFIG_FILES})

add_compile_definitions(UNIT_TESTING)

# Set the components to include the tests for.
# This can be overriden from CMake cache:
# - when invoking CMake directly: cmake -D TEST_COMPONENTS="xxxxx" ..
# - when using idf.py: idf.py -T xxxxx build
#
set(TEST_COMPONENTS "F7Services" CACHE STRING "List of components to test")

#
#   Now work out which compiler defines to set to turn various
#   features on or off (e.g. WiFi logging etc.).
#
if (DEBUG EQUAL 1)
    add_compile_definitions(DEBUG)
endif()

if (TRACE_MESSAGES EQUAL 1)
    add_compile_definitions(TRACE_MESSAGES)
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

if (MESSAGES_DEBUG EQUAL 1)
    add_compile_definitions(MESSAGES_DEBUG)
endif()

if (BT_DEBUG EQUAL 1)
    add_compile_definitions(BT_DEBUG)
endif()

if (THREAD_DEBUG EQUAL 1)
    add_compile_definitions(THREAD_DEBUG)
endif()

if (FILE_SYSTEM_DEBUG EQUAL 1)
    add_compile_definitions(FILE_SYSTEM_DEBUG)
endif()

if (PERFORMANCE_LOGGING EQUAL 1)
    add_compile_definitions(PERFORMANCE_LOGGING)
endif()

if (HEAP_SIZE GREATER 0)
    add_compile_definitions(HEAP_SIZE=${HEAP_SIZE})
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MeadowComms)
