cmake_minimum_required(VERSION 3.16)

project(EOS_UI VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(EOS_UI_SOURCES
    mainwindow.cpp
    mainwindow.h
    ui/acquisitionpanel.cpp
    ui/acquisitionpanel.h
    ui/appstyle.cpp
    ui/appstyle.h
    ui/dicomdialog.cpp
    ui/dicomdialog.h
    ui/imagingviewport.cpp
    ui/imagingviewport.h
    ui/inspectorpanel.cpp
    ui/inspectorpanel.h
    ui/safetylockdialog.cpp
    ui/safetylockdialog.h
    ui/viewportgrid.cpp
    ui/viewportgrid.h
    ui/workflowpages.cpp
    ui/workflowpages.h
)

add_library(eos_ui_components STATIC ${EOS_UI_SOURCES})
target_include_directories(eos_ui_components PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(eos_ui_components PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)

add_executable(EOS_UI WIN32 main.cpp)
target_link_libraries(EOS_UI PRIVATE eos_ui_components)

option(EOS_UI_BUILD_TESTS "Build UI smoke tests" ON)
if (EOS_UI_BUILD_TESTS)
    enable_testing()
    add_executable(EOS_UI_smoke tests/ui_smoke.cpp)
    target_link_libraries(EOS_UI_smoke PRIVATE eos_ui_components)
    add_executable(EOS_UI_capture tests/ui_capture.cpp)
    target_link_libraries(EOS_UI_capture PRIVATE eos_ui_components)
    add_test(NAME ui_smoke COMMAND EOS_UI_smoke)
    set_tests_properties(ui_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if (MSVC)
    target_compile_options(eos_ui_components PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(EOS_UI PRIVATE /W4 /permissive- /utf-8)
    if (TARGET EOS_UI_smoke)
        target_compile_options(EOS_UI_smoke PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(EOS_UI_capture PRIVATE /W4 /permissive- /utf-8)
    endif()
else()
    target_compile_options(eos_ui_components PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(EOS_UI PRIVATE -Wall -Wextra -Wpedantic)
    if (TARGET EOS_UI_smoke)
        target_compile_options(EOS_UI_smoke PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(EOS_UI_capture PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
