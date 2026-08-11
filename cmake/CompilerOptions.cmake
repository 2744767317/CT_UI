include_guard(GLOBAL)

function(ct_apply_compiler_options target)
    # 所有目标使用一致的告警级别和 UTF-8 源码解释方式，避免中文界面文本因
    # MSVC 系统代码页不同而发生乱码。
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
            /FS
            /external:anglebrackets
            /external:W0
        )
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
    )
endfunction()

function(ct_apply_linker_options target)
    if(MSVC AND CT_ENABLE_MEDICAL_BACKEND)
        # 当前 ITK Debug 安装缺少部分三方静态库 PDB；只忽略该已知符号警告，
        # 其他链接器告警仍完整保留。
        target_link_options(${target} PRIVATE /ignore:4099)
    endif()
endfunction()
function(ct_set_output_directories target subdirectory)
    # 单配置和多配置生成器统一输出到 artifacts/<配置>/，不向源码目录写入产物。
    set(_ct_output_root "${CMAKE_BINARY_DIR}/artifacts/$<CONFIG>")
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_ct_output_root}/${subdirectory}"
        LIBRARY_OUTPUT_DIRECTORY "${_ct_output_root}/lib"
        ARCHIVE_OUTPUT_DIRECTORY "${_ct_output_root}/lib"
    )
endfunction()
