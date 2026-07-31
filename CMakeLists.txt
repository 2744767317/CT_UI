cmake_minimum_required(VERSION 3.16)

project(EOS_UI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_executable(EOS_UI WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(EOS_UI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if (MSVC)
    target_compile_options(EOS_UI PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(EOS_UI PRIVATE -Wall -Wextra -Wpedantic)
endif()
