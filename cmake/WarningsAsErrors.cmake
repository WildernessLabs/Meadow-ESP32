#
#   Make the compiler warnings in our own code into errors.
#
#   ESP-IDF builds every component with -Werror=all but then turns a handful of warnings back
#   into warnings (-Wno-error=unused-variable and the like), which is how a warning in our code
#   went unnoticed in a release build.  Components we do not own, such as the managed components
#   under managed_components, are not given these options and keep the ESP-IDF settings.
#
#   The warnings ESP-IDF demotes are named here as well as -Werror, because a warning named
#   with -Wno-error= stays a warning whatever -Werror says, and the later option wins.
#
#   Usage, after idf_component_register():
#
#       include(${CMAKE_CURRENT_LIST_DIR}/<relative path to>/cmake/WarningsAsErrors.cmake)
#       meadow_warnings_as_errors(${COMPONENT_LIB})
#
function(meadow_warnings_as_errors target)
    target_compile_options(${target} PRIVATE
        -Werror
        -Werror=unused-variable
        -Werror=unused-function
        -Werror=unused-but-set-variable
        -Werror=deprecated-declarations
        -Werror=extra
    )
endfunction()
