cmake_minimum_required(VERSION 3.21)

project(RecompStation
    VERSION 0.1.0
    DESCRIPTION "Native recompilation game library frontend"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Sql
)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(RecompStationApp
    src/main.cpp
    src/core/PlatformManager.h
    src/core/PlatformManager.cpp
    src/core/LibraryScanner.h
    src/core/LibraryScanner.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
)

qt_add_qml_module(RecompStationApp
    URI RecompStation
    VERSION 1.0
    QML_FILES qml/Main.qml
)

target_link_libraries(RecompStationApp PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Sql
)

install(TARGETS RecompStationApp RUNTIME DESTINATION bin)
