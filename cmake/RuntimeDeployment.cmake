include_guard(GLOBAL)

function(ct_copy_medical_runtime_dlls target)
    if(WIN32 AND CT_ENABLE_MEDICAL_BACKEND)
        # TARGET_RUNTIME_DLLS 由 CMake 根据真实链接关系计算，仅部署当前目标实际
        # 使用的医学 DLL；Qt 运行库仍由 Qt Creator 或 windeployqt 负责。
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<FILTER:$<TARGET_RUNTIME_DLLS:${target}>,INCLUDE,.*/(vtk|ITK|itk|RTK|rtk)[^/]*[.]dll$>"
                $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying linked VTK/ITK/RTK runtime DLLs for ${target}"
        )
    endif()
endfunction()
