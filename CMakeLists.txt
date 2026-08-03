cmake_minimum_required(VERSION 3.24)

project(CT_UI VERSION 0.3 LANGUAGES CXX)

if(NOT WIN32 OR (NOT MSVC AND NOT MINGW))
    message(FATAL_ERROR "CT_UI currently supports Windows MSVC and MinGW toolchains.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "CT_UI requires a 64-bit toolchain.")
endif()

if(MSVC AND DEFINED CMAKE_VS_PLATFORM_TOOLSET AND
   NOT CMAKE_VS_PLATFORM_TOOLSET MATCHES "^v143")
    message(WARNING
        "The checked-in MSVC preset uses v143, which is the validated baseline for the supplied VTK/ITK and CUDA 12.8 environment.")
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
    find_package(VTK 9.5 REQUIRED COMPONENTS
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
    find_package(ITK 5.4 REQUIRED COMPONENTS
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
