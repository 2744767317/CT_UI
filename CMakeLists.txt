cmake_minimum_required(VERSION 3.24)

project(CT_UI VERSION 0.3 LANGUAGES CXX)

# 项目级 CMake 只负责总装；依赖发现、编译选项、输出目录和源码清单分别放在
# cmake/ 下，测试目标放在 tests/ 下，避免根文件持续膨胀。
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(MedicalDependencies)
include(RuntimeDeployment)
include(SourceFiles)

if(NOT WIN32 OR (NOT MSVC AND NOT MINGW))
    message(FATAL_ERROR "CT_UI currently supports Windows MSVC and MinGW toolchains.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "CT_UI requires a 64-bit toolchain.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Concurrent
    Test
)

# 该函数根据 CT_ENABLE_MEDICAL_BACKEND 决定是否加载 VTK/ITK/RTK。
# 外部 SDK 始终从安装目录引用，不会复制到源码仓库。
ct_configure_medical_dependencies()

set(CT_UI_CPP_SOURCES ${CT_UI_COMMON_SOURCES})

if(CT_ENABLE_MEDICAL_BACKEND)
    list(APPEND CT_UI_CPP_SOURCES ${CT_UI_MEDICAL_SOURCES})
else()
    list(APPEND CT_UI_CPP_SOURCES ${CT_UI_COMPATIBILITY_SOURCES})
endif()

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(CT_UI WIN32
    src/main.cpp
    ${CT_UI_CPP_SOURCES}
)
ct_set_output_directories(CT_UI bin)

set_source_files_properties(qml/theme/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(CT_UI
    URI GuangSuo.CT
    VERSION 1.0
    QML_FILES ${CT_UI_QML_FILES}
)

target_include_directories(CT_UI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
ct_apply_compiler_options(CT_UI)
ct_copy_medical_runtime_dlls(CT_UI)

target_link_libraries(CT_UI PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
)

if(CT_ENABLE_MEDICAL_BACKEND)
    target_compile_definitions(CT_UI PRIVATE CT_ENABLE_MEDICAL_BACKEND=1)
    target_link_libraries(CT_UI PRIVATE ${ITK_LIBRARIES} ${VTK_LIBRARIES})
    vtk_module_autoinit(
        TARGETS CT_UI
        MODULES ${VTK_LIBRARIES}
    )
else()
    target_compile_definitions(CT_UI PRIVATE CT_ENABLE_MEDICAL_BACKEND=0)
endif()

option(CT_UI_BUILD_TESTS "Build CT_UI automated tests" ON)
if(CT_UI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS CT_UI
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
