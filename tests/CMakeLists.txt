# 测试目标与正式程序分开声明。它复用同一个控制器实现，但不链接 QML 和 VTK
# 渲染层，因此核心状态机、DICOM 和 ITK 测试启动更快。
qt_add_executable(CT_UI_core_tests
    core_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/application/workflowcontroller.cpp
    ${PROJECT_SOURCE_DIR}/src/application/workflowcontroller.h
    $<$<BOOL:${CT_ENABLE_MEDICAL_BACKEND}>:${PROJECT_SOURCE_DIR}/src/dicom/medicaldatacontroller.cpp>
    $<$<NOT:$<BOOL:${CT_ENABLE_MEDICAL_BACKEND}>>:${PROJECT_SOURCE_DIR}/src/dicom/medicaldatacontroller_stub.cpp>
    ${PROJECT_SOURCE_DIR}/src/dicom/medicaldatacontroller.h
)

ct_set_output_directories(CT_UI_core_tests tests)
target_include_directories(CT_UI_core_tests PRIVATE ${PROJECT_SOURCE_DIR})
ct_apply_compiler_options(CT_UI_core_tests)
target_compile_definitions(CT_UI_core_tests PRIVATE
    CT_ENABLE_MEDICAL_BACKEND=$<BOOL:${CT_ENABLE_MEDICAL_BACKEND}>
)
target_link_libraries(CT_UI_core_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    Qt6::Test
)

if(CT_ENABLE_MEDICAL_BACKEND)
    target_link_libraries(CT_UI_core_tests PRIVATE ${ITK_LIBRARIES})
endif()

ct_copy_medical_runtime_dlls(CT_UI_core_tests)
add_test(NAME core_tests COMMAND CT_UI_core_tests)

# CTest 直接启动可执行文件时没有 Qt Creator 的运行环境，需要显式补充 DLL 路径。
if(MINGW)
    set_tests_properties(core_tests PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${Qt6_DIR}/../../../bin"
    )
elseif(MSVC AND CT_ENABLE_MEDICAL_BACKEND)
    set_tests_properties(core_tests PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${VTK_DIR}/../../../bin;PATH=path_list_prepend:${ITK_DIR}/../../../bin;PATH=path_list_prepend:${Qt6_DIR}/../../../bin"
    )
endif()
