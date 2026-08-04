include_guard(GLOBAL)

function(ct_apply_compiler_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
    )
endfunction()
function(ct_set_output_directories target subdirectory)
    set(_ct_output_root "${CMAKE_BINARY_DIR}/artifacts/$<CONFIG>")
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_ct_output_root}/${subdirectory}"
        LIBRARY_OUTPUT_DIRECTORY "${_ct_output_root}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${_ct_output_root}/lib"
    )
endfunction()
