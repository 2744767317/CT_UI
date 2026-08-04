include_guard(GLOBAL)

function(ct_copy_medical_runtime_dlls target)
    if(WIN32 AND CT_ENABLE_MEDICAL_BACKEND)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<FILTER:$<TARGET_RUNTIME_DLLS:${target}>,INCLUDE,.*/(vtk|ITK|itk|RTK|rtk)[^/]*[.]dll$>"
                $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying linked VTK/ITK/RTK runtime DLLs for ${target}"
        )
    endif()
endfunction()
