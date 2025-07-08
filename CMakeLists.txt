cmake_minimum_required(VERSION 3.16)
project(wallpaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(LayerShellQt REQUIRED)

add_executable(qt-wall-xml main.cpp)

target_include_directories(qt-wall-xml PRIVATE /usr/include/LayerShellQt)
target_link_libraries(qt-wall-xml PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    /usr/lib/libLayerShellQtInterface.so
)
