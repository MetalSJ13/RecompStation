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
    Core Gui Quick QuickControls2 Sql Network Concurrent Svg ShaderTools
)
find_package(LibArchive REQUIRED)

# SDL2 is optional: builds without it retain keyboard/mouse support.
find_package(SDL2 2.0.14 QUIET)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(RecompStationApp
    src/main.cpp
    src/core/Logging.h
    src/core/Logging.cpp
    src/core/ControllerInputRouter.h
    src/core/ControllerInputRouter.cpp
    src/core/ControllerManager.h
    src/core/ControllerManager.cpp
    data/projects.qrc
    data/ui_icons.qrc
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
    src/core/LaunchPreparer.h
    src/core/LaunchPreparer.cpp
    src/core/RomInspector.h
    src/core/RomInspector.cpp
    src/core/ArchiveExtractor.h
    src/core/ArchiveExtractor.cpp
    src/core/LibraryScanner.h
    src/core/LibraryScanner.cpp
    src/core/LibraryMetadataMatcher.h
    src/core/LibraryMetadataMatcher.cpp
    src/core/ArtworkManager.h
    src/core/ArtworkManager.cpp
    src/core/LaunchBoxArtworkManager.h
    src/core/LaunchBoxArtworkManager.cpp
    src/core/ScreenshotManager.h
    src/core/ScreenshotManager.cpp
    src/core/StorageUsage.h
    src/core/StorageUsage.cpp
    src/core/GameImporter.h
    src/core/GameImporter.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/metadata/LaunchBoxXmlParser.h
    src/metadata/LaunchBoxXmlParser.cpp
    src/metadata/LaunchBoxCatalogMatcher.h
    src/metadata/LaunchBoxCatalogMatcher.cpp
    src/metadata/LaunchBoxCatalogDatabase.h
    src/metadata/LaunchBoxCatalogDatabase.cpp
    src/metadata/LaunchBoxArchiveExtractor.h
    src/metadata/LaunchBoxArchiveExtractor.cpp
    src/metadata/LaunchBoxCatalogManager.h
    src/metadata/LaunchBoxCatalogManager.cpp
    src/metadata/GameMetadata.h
    src/metadata/MetadataPlatformMapping.h
    src/metadata/MetadataProvider.h
    src/metadata/TheGamesDbClient.h
    src/metadata/TheGamesDbClient.cpp
    src/metadata/MetadataManager.h
    src/metadata/MetadataManager.cpp
    src/metadata/N64RomHash.h
    src/metadata/N64RomHash.cpp
)

qt_add_qml_module(RecompStationApp
    URI RecompStation
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/GameArtwork.qml
        qml/ArtworkLayout.js
        qml/LibraryCoverCard.qml
        qml/LibraryGridLayout.js
        qml/GameDetailsPage.qml
        qml/ScreenshotBackdrop.qml
        qml/ScreenshotViewer.qml
        qml/ValueCycler.qml
        qml/SettingRow.qml
        qml/SettingsGroupLabel.qml
        qml/SettingsNavItem.qml
        qml/ThemeTile.qml
        qml/BackgroundStyleTile.qml
        qml/WallpaperBackground.qml
        qml/WaveBackground.qml
        qml/ColorSwatchPicker.qml
        qml/StatusPill.qml
        qml/ToggleSwitch.qml
        qml/ControllerShapes.js
        qml/ControllerOutline.qml
        qml/ImportGamesPanel.qml
        qml/FirstRunSetup.qml
        qml/LaunchBoxProgress.qml
        qml/LaunchOverlay.qml
    RESOURCES
        assets/logo/recompstation-mark.svg
        assets/logo/png/recompstation-icon-16.png
        assets/logo/png/recompstation-icon-24.png
        assets/logo/png/recompstation-icon-32.png
        assets/logo/png/recompstation-icon-48.png
        assets/logo/png/recompstation-icon-64.png
        assets/logo/png/recompstation-icon-128.png
        assets/logo/png/recompstation-icon-256.png
)

# Library wave background shader, compiled to .qsb (Qt Shader Tools) and
# served as qrc:/shaders/waves.frag.qsb.
qt_add_shaders(RecompStationApp "recompstation_shaders"
    PREFIX "/"
    FILES
        shaders/waves.frag
)

# GCC 16 enables -Wsfinae-incomplete by default. Qt 6's generated moc code
# can trigger this diagnostic while probing QObject-derived types; it is a
# toolchain/Qt meta-object warning rather than an application type error.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16")
    target_compile_options(RecompStationApp PRIVATE -Wno-sfinae-incomplete)
endif()

# xxHash (BSD 2-Clause, header only): checks N64 ROMs against launcher checksums.
target_include_directories(RecompStationApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xxhash
)

target_link_libraries(RecompStationApp PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Network Qt6::Concurrent Qt6::Svg
    LibArchive::LibArchive
)

if(TARGET SDL2::SDL2)
    target_link_libraries(RecompStationApp PRIVATE SDL2::SDL2)
    target_compile_definitions(RecompStationApp PRIVATE RECOMPSTATION_HAS_SDL2)
    message(STATUS "SDL2 controller support enabled")
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(RecompStationApp PRIVATE SDL2::SDL2-static)
    target_compile_definitions(RecompStationApp PRIVATE RECOMPSTATION_HAS_SDL2)
    message(STATUS "SDL2 controller support enabled")
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2_PC QUIET IMPORTED_TARGET sdl2>=2.0.14)
        if(TARGET PkgConfig::SDL2_PC)
            target_link_libraries(RecompStationApp PRIVATE PkgConfig::SDL2_PC)
            target_compile_definitions(RecompStationApp PRIVATE RECOMPSTATION_HAS_SDL2)
            message(STATUS "SDL2 controller support enabled via pkg-config")
        endif()
    endif()
    if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static
       AND NOT TARGET PkgConfig::SDL2_PC)
        message(STATUS "SDL2 >= 2.0.14 not found; controller support disabled")
    endif()
endif()

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test QuickTest)

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

    qt_add_executable(ControllerInputRouterTests
        tests/ControllerInputRouterTest.cpp
        src/core/ControllerInputRouter.h
        src/core/ControllerInputRouter.cpp
    )

    target_include_directories(ControllerInputRouterTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ControllerInputRouterTests PRIVATE
        Qt6::Core Qt6::Test
    )

    add_test(NAME ControllerInputRouterTests COMMAND ControllerInputRouterTests)

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

    qt_add_executable(GameImporterTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/GameImporterTest.cpp
        src/core/GameImporter.h
        src/core/GameImporter.cpp
        src/core/PlatformManager.h
        src/core/PlatformManager.cpp
    )

    target_include_directories(GameImporterTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(GameImporterTests PRIVATE
        Qt6::Core Qt6::Concurrent Qt6::Test LibArchive::LibArchive
    )

    add_test(NAME GameImporterTests COMMAND GameImporterTests)

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
        data/projects.qrc
        src/core/GameLaunchManager.h
        src/core/GameLaunchManager.cpp
        src/core/LaunchPreparer.h
        src/core/LaunchPreparer.cpp
        src/core/RomInspector.h
        src/core/RomInspector.cpp
        src/metadata/N64RomHash.h
        src/metadata/N64RomHash.cpp
        src/projects/ProjectCatalog.h
        src/projects/ProjectCatalog.cpp
        src/core/SettingsManager.h
        src/core/SettingsManager.cpp
        src/core/LauncherDownloadManager.h
        src/core/LauncherDownloadManager.cpp
        src/core/ArchiveExtractor.h
        src/core/ArchiveExtractor.cpp
        src/core/PlatformManager.h
        src/core/PlatformManager.cpp
    )

    target_include_directories(GameLaunchManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xxhash
    )

    target_link_libraries(GameLaunchManagerTests PRIVATE
        Qt6::Core Qt6::Network Qt6::Concurrent Qt6::Test LibArchive::LibArchive
    )

    add_test(NAME GameLaunchManagerTests COMMAND GameLaunchManagerTests)

    qt_add_executable(LaunchPreparerTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/LaunchPreparerTest.cpp
        src/core/LaunchPreparer.h
        src/core/LaunchPreparer.cpp
        src/core/RomInspector.h
        src/core/RomInspector.cpp
        src/metadata/N64RomHash.h
        src/metadata/N64RomHash.cpp
    )

    target_include_directories(LaunchPreparerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xxhash
    )

    target_link_libraries(LaunchPreparerTests PRIVATE
        Qt6::Core Qt6::Test
    )

    add_test(NAME LaunchPreparerTests COMMAND LaunchPreparerTests)

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

    qt_add_executable(LaunchBoxArtworkManagerTests
        tests/LaunchBoxArtworkManagerTest.cpp
        src/core/LaunchBoxArtworkManager.h
        src/core/LaunchBoxArtworkManager.cpp
        src/core/Logging.h
        src/core/Logging.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
        src/metadata/LaunchBoxCatalogDatabase.h
        src/metadata/LaunchBoxCatalogDatabase.cpp
        src/metadata/LaunchBoxCatalogMatcher.h
        src/metadata/LaunchBoxCatalogMatcher.cpp
        src/metadata/LaunchBoxXmlParser.h
        src/metadata/LaunchBoxXmlParser.cpp
    )

    target_include_directories(LaunchBoxArtworkManagerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LaunchBoxArtworkManagerTests PRIVATE
        Qt6::Core Qt6::Sql Qt6::Network Qt6::Test
    )

    add_test(NAME LaunchBoxArtworkManagerTests COMMAND LaunchBoxArtworkManagerTests)

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

    qt_add_executable(LaunchBoxXmlParserTests
        tests/LaunchBoxXmlParserTest.cpp
        src/metadata/LaunchBoxXmlParser.h
        src/metadata/LaunchBoxXmlParser.cpp
    )

    target_include_directories(LaunchBoxXmlParserTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LaunchBoxXmlParserTests PRIVATE
        Qt6::Core Qt6::Test
    )

    add_test(NAME LaunchBoxXmlParserTests COMMAND LaunchBoxXmlParserTests)

    qt_add_executable(LaunchBoxCatalogMatcherTests
        tests/LaunchBoxCatalogMatcherTest.cpp
        src/metadata/LaunchBoxCatalogMatcher.h
        src/metadata/LaunchBoxCatalogMatcher.cpp
        src/metadata/LaunchBoxXmlParser.h
    )

    target_include_directories(LaunchBoxCatalogMatcherTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LaunchBoxCatalogMatcherTests PRIVATE
        Qt6::Core Qt6::Test
    )

    add_test(NAME LaunchBoxCatalogMatcherTests COMMAND LaunchBoxCatalogMatcherTests)

    qt_add_executable(LaunchBoxCatalogDatabaseTests
        tests/LaunchBoxCatalogDatabaseTest.cpp
        src/metadata/LaunchBoxCatalogDatabase.h
        src/metadata/LaunchBoxCatalogDatabase.cpp
        src/metadata/LaunchBoxCatalogMatcher.h
        src/metadata/LaunchBoxCatalogMatcher.cpp
        src/metadata/LaunchBoxXmlParser.h
        src/metadata/LaunchBoxXmlParser.cpp
    )

    target_include_directories(LaunchBoxCatalogDatabaseTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LaunchBoxCatalogDatabaseTests PRIVATE
        Qt6::Core Qt6::Sql Qt6::Test
    )

    add_test(NAME LaunchBoxCatalogDatabaseTests COMMAND LaunchBoxCatalogDatabaseTests)

    qt_add_executable(LaunchBoxArchiveExtractorTests
        tests/LaunchBoxArchiveExtractorTest.cpp
        src/metadata/LaunchBoxArchiveExtractor.h
        src/metadata/LaunchBoxArchiveExtractor.cpp
    )

    target_include_directories(LaunchBoxArchiveExtractorTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LaunchBoxArchiveExtractorTests PRIVATE
        Qt6::Core Qt6::Test LibArchive::LibArchive
    )

    add_test(NAME LaunchBoxArchiveExtractorTests COMMAND LaunchBoxArchiveExtractorTests)


    qt_add_executable(N64RomHashTests
        tests/N64RomHashTest.cpp
        src/metadata/N64RomHash.h
        src/metadata/N64RomHash.cpp
    )

    target_include_directories(N64RomHashTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(N64RomHashTests PRIVATE
        Qt6::Core Qt6::Test
    )

    add_test(NAME N64RomHashTests COMMAND N64RomHashTests)

    qt_add_executable(LibraryMetadataMatcherTests
        src/core/Logging.h
        src/core/Logging.cpp
        tests/LibraryMetadataMatcherTest.cpp
        src/core/LibraryMetadataMatcher.h
        src/core/LibraryMetadataMatcher.cpp
        src/core/ArtworkManager.h
        src/core/ArtworkManager.cpp
        src/core/SettingsManager.h
        src/core/SettingsManager.cpp
        src/core/ArchiveExtractor.h
        src/core/ArchiveExtractor.cpp
        src/database/DatabaseManager.h
        src/database/DatabaseManager.cpp
        src/metadata/GameMetadata.h
        src/metadata/MetadataPlatformMapping.h
        src/metadata/MetadataProvider.h
        src/metadata/MetadataManager.h
        src/metadata/MetadataManager.cpp
        src/metadata/LaunchBoxXmlParser.h
        src/metadata/LaunchBoxXmlParser.cpp
        src/metadata/LaunchBoxCatalogMatcher.h
        src/metadata/LaunchBoxCatalogMatcher.cpp
        src/metadata/LaunchBoxCatalogDatabase.h
        src/metadata/LaunchBoxCatalogDatabase.cpp
        src/metadata/LaunchBoxArchiveExtractor.h
        src/metadata/LaunchBoxArchiveExtractor.cpp
        src/metadata/LaunchBoxCatalogManager.h
        src/metadata/LaunchBoxCatalogManager.cpp
    )

    target_include_directories(LibraryMetadataMatcherTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(LibraryMetadataMatcherTests PRIVATE
        Qt6::Core Qt6::Sql Qt6::Network Qt6::Concurrent Qt6::Test LibArchive::LibArchive
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

    # QML-side logic tests. Each tst_*.qml file in tests/qml runs as a test case.
    qt_add_executable(ArtworkLayoutTests
        tests/qml/ArtworkLayoutTestMain.cpp
    )

    target_compile_definitions(ArtworkLayoutTests PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/qml"
    )

    target_link_libraries(ArtworkLayoutTests PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickTest
    )

    add_test(NAME ArtworkLayoutTests COMMAND ArtworkLayoutTests)
    # Run without opening a window on the developer's desktop.
    set_tests_properties(ArtworkLayoutTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

install(TARGETS RecompStationApp RUNTIME DESTINATION bin)
