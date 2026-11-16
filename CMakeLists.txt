cmake_minimum_required(VERSION 3.21)

project(RecompStation
    VERSION 0.1.0
    DESCRIPTION "Native recompilation game library frontend"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compile Qt .qrc files into every target that lists them as a source.
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Sql Network Concurrent
)
find_package(LibArchive REQUIRED)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(RecompStationApp
    src/main.cpp
    src/core/Logging.h
    src/core/Logging.cpp
    data/projects.qrc
    src/projects/ProjectCatalog.h
    src/projects/ProjectCatalog.cpp
    src/github/GitHubManager.h
    src/github/GitHubManager.cpp
    src/core/PlatformManager.h
    src/core/PlatformManager.cpp
    src/core/SettingsManager.h
    src/core/SettingsManager.cpp
    src/core/LauncherDownloadManager.h
    src/core/LauncherDownloadManager.cpp
    src/core/GameLaunchManager.h
    src/core/GameLaunchManager.cpp
    src/core/ArchiveExtractor.h
    src/core/ArchiveExtractor.cpp
    src/core/LibraryScanner.h
    src/core/LibraryScanner.cpp
    src/core/LibraryMetadataMatcher.h
    src/core/LibraryMetadataMatcher.cpp
    src/core/ArtworkManager.h
    src/core/ArtworkManager.cpp
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
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
)

target_link_libraries(RecompStationApp PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Network Qt6::Concurrent
    LibArchive::LibArchive
)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    qt_add_executable(ArchiveExtractorTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/ArchiveExtractorTest.cpp
        src/core/ArchiveExtractor.h
        src/core/ArchiveExtractor.cpp
    )

    target_include_directories(ArchiveExtractorTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ArchiveExtractorTests PRIVATE
        Qt6::Core Qt6::Test LibArchive::LibArchive
    )

    add_test(NAME ArchiveExtractorTests COMMAND ArchiveExtractorTests)

    qt_add_executable(SettingsManagerTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/SettingsManagerTest.cpp
        src/core/SettingsManager.h
        src/core/SettingsManager.cpp
    )

    target_include_directories(SettingsManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(SettingsManagerTests PRIVATE
        Qt6::Core Qt6::Test
    )

    add_test(NAME SettingsManagerTests COMMAND SettingsManagerTests)

    qt_add_executable(PlatformManagerTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/PlatformManagerTest.cpp
        src/core/PlatformManager.h
        src/core/PlatformManager.cpp
    )

    target_include_directories(PlatformManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(PlatformManagerTests PRIVATE
        Qt6::Core Qt6::Test
    )

    add_test(NAME PlatformManagerTests COMMAND PlatformManagerTests)

    qt_add_executable(LauncherDownloadManagerTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/LauncherDownloadManagerTest.cpp
        src/core/LauncherDownloadManager.h
        src/core/LauncherDownloadManager.cpp
        src/core/ArchiveExtractor.h
        src/core/ArchiveExtractor.cpp
        src/core/PlatformManager.h
        src/core/PlatformManager.cpp
    )

    target_include_directories(LauncherDownloadManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LauncherDownloadManagerTests PRIVATE
        Qt6::Core Qt6::Network Qt6::Concurrent Qt6::Test LibArchive::LibArchive
    )

    add_test(NAME LauncherDownloadManagerTests COMMAND LauncherDownloadManagerTests)

    qt_add_executable(GameLaunchManagerTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/GameLaunchManagerTest.cpp
        src/core/GameLaunchManager.h
        src/core/GameLaunchManager.cpp
        src/core/LauncherDownloadManager.h
        src/core/LauncherDownloadManager.cpp
        src/core/ArchiveExtractor.h
        src/core/ArchiveExtractor.cpp
        src/core/PlatformManager.h
        src/core/PlatformManager.cpp
    )

    target_include_directories(GameLaunchManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(GameLaunchManagerTests PRIVATE
        Qt6::Core Qt6::Network Qt6::Concurrent Qt6::Test LibArchive::LibArchive
    )

    add_test(NAME GameLaunchManagerTests COMMAND GameLaunchManagerTests)

    qt_add_executable(LibraryScannerTests
        src/core/Logging.h
        src/core/Logging.cpp
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
        src/core/Logging.h
        src/core/Logging.cpp
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

    qt_add_executable(ArtworkManagerTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/ArtworkManagerTest.cpp
        src/core/ArtworkManager.h
        src/core/ArtworkManager.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
    )

    target_include_directories(ArtworkManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ArtworkManagerTests PRIVATE
        Qt6::Core Qt6::Sql Qt6::Network Qt6::Test
    )

    add_test(NAME ArtworkManagerTests COMMAND ArtworkManagerTests)

    qt_add_executable(MetadataProviderTests
        src/core/Logging.h
        src/core/Logging.cpp
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

    qt_add_executable(LibraryMetadataMatcherTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/LibraryMetadataMatcherTest.cpp
        src/core/LibraryMetadataMatcher.h
        src/core/LibraryMetadataMatcher.cpp
        src/core/ArtworkManager.h
        src/core/ArtworkManager.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
        src/metadata/GameMetadata.h
        src/metadata/MetadataPlatformMapping.h
        src/metadata/MetadataProvider.h
        src/metadata/MetadataManager.h
        src/metadata/MetadataManager.cpp
    )

    target_include_directories(LibraryMetadataMatcherTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LibraryMetadataMatcherTests PRIVATE
        Qt6::Core Qt6::Sql Qt6::Network Qt6::Test
    )

    add_test(NAME LibraryMetadataMatcherTests COMMAND LibraryMetadataMatcherTests)

    qt_add_executable(ProjectCatalogTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/ProjectCatalogTest.cpp
        data/projects.qrc
        src/projects/ProjectCatalog.h
        src/projects/ProjectCatalog.cpp
    )

    target_include_directories(ProjectCatalogTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ProjectCatalogTests PRIVATE
        Qt6::Core Qt6::Test
    )

    add_test(NAME ProjectCatalogTests COMMAND ProjectCatalogTests)

    qt_add_executable(GitHubManagerTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/GitHubManagerTest.cpp
        tests/GitHubManagerTest.h
        src/github/GitHubManager.h
        src/github/GitHubManager.cpp
    )

    target_include_directories(GitHubManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(GitHubManagerTests PRIVATE
        Qt6::Core Qt6::Network Qt6::Test
    )

    add_test(NAME GitHubManagerTests COMMAND GitHubManagerTests)
endif()

install(TARGETS RecompStationApp RUNTIME DESTINATION bin)
