cmake_minimum_required(VERSION 3.24)

project(CT_UI VERSION 0.3 LANGUAGES CXX)

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

set(_ct_backend_default ON)
if(MINGW)
    set(_ct_backend_default OFF)
endif()
option(CT_ENABLE_MEDICAL_BACKEND
    "Enable DICOM, ITK segmentation, and VTK rendering" ${_ct_backend_default})

if(CT_ENABLE_MEDICAL_BACKEND)
    if(MSVC AND DEFINED MSVC_TOOLSET_VERSION AND
       NOT MSVC_TOOLSET_VERSION STREQUAL "143")
        message(WARNING
            "MSVC v${MSVC_TOOLSET_VERSION} is selected. The current DICOM/ITK/VTK workstation is compatible, but CUDA 12.8 and the future RTK GPU backend require the v143 release preset.")
    endif()

    set(CT_MEDICAL_SDK_ROOT "E:/A/GuangSuo" CACHE PATH
        "Root containing the local VTK_INSTALL and ITK_INSTALL directories")
    set(CT_VTK_INSTALL_DIR "${CT_MEDICAL_SDK_ROOT}/VTK_INSTALL/install_debug" CACHE PATH
        "VTK installation prefix used by the medical backend")
    set(CT_ITK_INSTALL_DIR "${CT_MEDICAL_SDK_ROOT}/ITK_INSTALL/install_debug" CACHE PATH
        "ITK/RTK installation prefix used by the medical backend")

    if(NOT VTK_DIR OR VTK_DIR MATCHES "-NOTFOUND$")
        set(_ct_vtk_config_candidates
            "$ENV{VTK_DIR}"
            "${CT_VTK_INSTALL_DIR}/lib/cmake/vtk-9.5"
        )
        foreach(_ct_candidate IN LISTS _ct_vtk_config_candidates)
            if(EXISTS "${_ct_candidate}/vtk-config.cmake" OR
               EXISTS "${_ct_candidate}/VTKConfig.cmake")
                set(VTK_DIR "${_ct_candidate}" CACHE PATH "VTK package directory" FORCE)
                break()
            endif()
        endforeach()
    endif()

    if(NOT ITK_DIR OR ITK_DIR MATCHES "-NOTFOUND$")
        set(_ct_itk_config_candidates
            "$ENV{ITK_DIR}"
            "${CT_ITK_INSTALL_DIR}/lib/cmake/ITK-5.4"
        )
        foreach(_ct_candidate IN LISTS _ct_itk_config_candidates)
            if(EXISTS "${_ct_candidate}/ITKConfig.cmake")
                set(ITK_DIR "${_ct_candidate}" CACHE PATH "ITK package directory" FORCE)
                break()
            endif()
        endforeach()
    endif()

    if(NOT EXISTS "${VTK_DIR}/vtk-config.cmake" OR
       NOT EXISTS "${VTK_DIR}/VTK-targets-debug.cmake")
        message(FATAL_ERROR
            "VTK_DIR must point to the complete VTK 9.5 Debug package containing vtk-config.cmake and VTK-targets-debug.cmake. Current value: ${VTK_DIR}")
    endif()
    if(NOT EXISTS "${ITK_DIR}/ITKConfig.cmake" OR
       NOT EXISTS "${ITK_DIR}/ITKTargets-debug.cmake")
        message(FATAL_ERROR
            "ITK_DIR must point to the complete ITK 5.4 Debug package containing ITKConfig.cmake and ITKTargets-debug.cmake. Current value: ${ITK_DIR}")
    endif()

    if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR
            "The supplied VTK installation contains Debug binaries only. Select a Debug build or provide a matching Release VTK installation.")
    endif()

    message(STATUS "CT medical SDK: VTK_DIR=${VTK_DIR}")
    message(STATUS "CT medical SDK: ITK_DIR=${ITK_DIR}")

    find_package(VTK 9.5 CONFIG REQUIRED COMPONENTS
        CommonCore
        CommonDataModel
        FiltersCore
        FiltersGeneral
        FiltersGeometry
        ImagingCore
        ImagingColor
        InteractionStyle
        RenderingCore
        RenderingOpenGL2
        RenderingVolumeOpenGL2
        GUISupportQtQuick
    )

    # This local ITK package config enumerates every enabled module before it
    # applies COMPONENTS. Its ITKVtkGlue module contains a stale absolute
    # VTK_DIR from the machine where ITK was built. CT_UI uses an explicit
    # snapshot bridge and does not link ITKVtkGlue, so prevent that unrelated
    # module from overwriting the validated VTK_DIR above.
    set(ITKVtkGlue_LOADED 1)
    set(TotalVariation_LOADED 1)
    set(RTK_LOADED 1)
    find_package(ITK 5.4 CONFIG REQUIRED COMPONENTS
        ITKCommon
        ITKIOGDCM
        ITKThresholding
        ITKRegionGrowing
    )
    include(${ITK_USE_FILE})
endif()

set(CT_UI_CPP_SOURCES
    src/application/workflowcontroller.cpp
    src/application/workflowcontroller.h
    src/dicom/medicaldatacontroller.h
    src/rendering/medicalviewportitem.h
)

if(CT_ENABLE_MEDICAL_BACKEND)
    list(APPEND CT_UI_CPP_SOURCES
        src/dicom/medicaldatacontroller.cpp
        src/rendering/medicalviewportitem.cpp
    )
else()
    list(APPEND CT_UI_CPP_SOURCES
        src/dicom/medicaldatacontroller_stub.cpp
        src/rendering/medicalviewportitem_stub.cpp
    )
endif()

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(CT_UI WIN32
    main.cpp
    ${CT_UI_CPP_SOURCES}
)
set_target_properties(CT_UI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

function(ct_copy_runtime_dlls target)
    if(WIN32 AND CT_ENABLE_MEDICAL_BACKEND)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<FILTER:$<TARGET_RUNTIME_DLLS:${target}>,INCLUDE,.*/(vtk|ITK|itk)[^/]*[.]dll$>"
                $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying VTK/ITK runtime DLL dependencies for ${target}"
        )
    endif()
endfunction()

set_source_files_properties(qml/theme/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(CT_UI
    URI GuangSuo.CT
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/ActionButton.qml
        qml/components/StatusPill.qml
        qml/components/ViewportPane.qml
        qml/pages/PatientPage.qml
        qml/pages/SafetyPage.qml
        qml/pages/ScanRangePage.qml
        qml/pages/WorkstationPage.qml
        qml/workstation/DataPanel.qml
        qml/workstation/InspectorPanel.qml
)

target_include_directories(CT_UI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(CT_UI PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(CT_UI PRIVATE -Wall -Wextra -Wpedantic)
endif()
ct_copy_runtime_dlls(CT_UI)
target_compile_definitions(CT_UI PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

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
    message(STATUS "CT_UI medical backend disabled: building portable QML/UI mode")
endif()

option(CT_UI_BUILD_TESTS "Build CT_UI automated tests" ON)
if(CT_UI_BUILD_TESTS)
    enable_testing()
    qt_add_executable(CT_UI_core_tests
        tests/core_tests.cpp
        src/application/workflowcontroller.cpp
        src/application/workflowcontroller.h
        $<$<BOOL:${CT_ENABLE_MEDICAL_BACKEND}>:src/dicom/medicaldatacontroller.cpp>
        $<$<NOT:$<BOOL:${CT_ENABLE_MEDICAL_BACKEND}>>:src/dicom/medicaldatacontroller_stub.cpp>
        src/dicom/medicaldatacontroller.h
    )
    set_target_properties(CT_UI_core_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
    )
    target_include_directories(CT_UI_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(CT_UI_core_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(CT_UI_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_compile_definitions(CT_UI_core_tests PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        CT_ENABLE_MEDICAL_BACKEND=$<BOOL:${CT_ENABLE_MEDICAL_BACKEND}>
    )
    target_link_libraries(CT_UI_core_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    if(CT_ENABLE_MEDICAL_BACKEND)
        target_link_libraries(CT_UI_core_tests PRIVATE ${ITK_LIBRARIES})
    endif()
    ct_copy_runtime_dlls(CT_UI_core_tests)
    add_test(NAME core_tests COMMAND CT_UI_core_tests)
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
endif()

install(TARGETS CT_UI
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
