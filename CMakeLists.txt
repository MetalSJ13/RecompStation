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
    Core Gui Quick QuickControls2 Sql Network
)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(RecompStationApp
    src/main.cpp
    src/core/PlatformManager.h
    src/core/PlatformManager.cpp
    src/core/LibraryScanner.h
    src/core/LibraryScanner.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/metadata/GameMetadata.h
    src/metadata/MetadataPlatformMapping.h
    src/metadata/MetadataProvider.h
    src/metadata/TheGamesDbClient.h
    src/metadata/TheGamesDbClient.cpp
    src/metadata/MetadataManager.h
    src/metadata/MetadataManager.cpp
)

qt_add_qml_module(RecompStationApp
    URI RecompStation
    VERSION 1.0
    QML_FILES qml/Main.qml
)

target_link_libraries(RecompStationApp PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Network
)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(LibraryScannerTests
        tests/LibraryScannerTest.cpp
        src/core/PlatformManager.h
        src/core/PlatformManager.cpp
        src/core/LibraryScanner.h
        src/core/LibraryScanner.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
    )

    target_include_directories(LibraryScannerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LibraryScannerTests PRIVATE
        Qt6::Core Qt6::Sql Qt6::Test
    )

    add_test(NAME LibraryScannerTests COMMAND LibraryScannerTests)

    qt_add_executable(DatabaseManagerTests
        tests/DatabaseManagerTest.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
    )

    target_include_directories(DatabaseManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(DatabaseManagerTests PRIVATE
        Qt6::Core Qt6::Sql Qt6::Test
    )

    add_test(NAME DatabaseManagerTests COMMAND DatabaseManagerTests)

    qt_add_executable(MetadataProviderTests
        tests/MetadataProviderTest.cpp
        src/metadata/GameMetadata.h
        src/metadata/MetadataPlatformMapping.h
        src/metadata/MetadataProvider.h
        src/metadata/TheGamesDbClient.h
        src/metadata/TheGamesDbClient.cpp
        src/metadata/MetadataManager.h
        src/metadata/MetadataManager.cpp
    )

    target_include_directories(MetadataProviderTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(MetadataProviderTests PRIVATE
        Qt6::Core Qt6::Network Qt6::Test
    )

    add_test(NAME MetadataProviderTests COMMAND MetadataProviderTests)
endif()

install(TARGETS RecompStationApp RUNTIME DESTINATION bin)
