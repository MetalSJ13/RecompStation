cmake_minimum_required(VERSION 3.21)

project(RecompStation
    VERSION 0.1.1
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

# SDL2 is optional: it reads controllers and plays the interface sounds.
# Builds without it keep keyboard/mouse support and stay silent.
find_package(SDL2 2.0.14 QUIET)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

# Build speed: a compiler cache when one is installed (set
# CMAKE_CXX_COMPILER_LAUNCHER yourself to override), and a faster linker
# (mold, then lld) when the compiler accepts it. RECOMPSTATION_FAST_LINKER=OFF
# keeps the default linker.
if(NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(RECOMPSTATION_CCACHE ccache)
    if(RECOMPSTATION_CCACHE)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${RECOMPSTATION_CCACHE}")
        message(STATUS "Compiler cache: ${RECOMPSTATION_CCACHE}")
    endif()
endif()

option(RECOMPSTATION_FAST_LINKER "Link with mold or lld when available" ON)
if(RECOMPSTATION_FAST_LINKER AND NOT MSVC)
    include(CheckLinkerFlag)
    check_linker_flag(CXX "-fuse-ld=mold" RECOMPSTATION_HAVE_MOLD)
    if(RECOMPSTATION_HAVE_MOLD)
        add_link_options("-fuse-ld=mold")
        message(STATUS "Linker: mold")
    else()
        check_linker_flag(CXX "-fuse-ld=lld" RECOMPSTATION_HAVE_LLD)
        if(RECOMPSTATION_HAVE_LLD)
            add_link_options("-fuse-ld=lld")
            message(STATUS "Linker: lld")
        endif()
    endif()
endif()

# Everything except main.cpp and the SDL controller backend, compiled once
# and shared by the app and every test (each test used to recompile the
# sources it needed: 98 compilations of 29 files).
qt_add_library(recompstation_core STATIC
    src/core/Logging.h
    src/core/Logging.cpp
    src/core/UiSoundGate.h
    src/core/UiSoundGate.cpp
    src/core/ControllerInputRouter.h
    src/core/ControllerInputRouter.cpp
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
    src/core/LaunchPreparer.h
    src/core/LaunchPreparer.cpp
    src/core/RomInspector.h
    src/core/RomInspector.cpp
    src/core/RomArchiveUnpacker.h
    src/core/RomArchiveUnpacker.cpp
    src/core/ArchiveExtractor.h
    src/core/ArchiveExtractor.cpp
    src/core/AppUpdater.h
    src/core/AppUpdater.cpp
    src/core/WineSupport.h
    src/core/WineSupport.cpp
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
    src/metadata/TheGamesDbAllowance.h
    src/metadata/TheGamesDbAllowance.cpp
    src/metadata/MetadataManager.h
    src/metadata/MetadataManager.cpp
    src/metadata/N64RomHash.h
    src/metadata/N64RomHash.cpp
)

target_include_directories(recompstation_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# xxHash (BSD 2-Clause, header only): checks N64 ROMs against launcher checksums.
target_include_directories(recompstation_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xxhash
)
target_link_libraries(recompstation_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Sql Qt6::Network Qt6::Concurrent
    LibArchive::LibArchive
)

# Commit the app was built from, shown at the top of each session in the log.
find_package(Git QUIET)
set(RECOMPSTATION_BUILD_ID "unknown")
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --always --dirty --abbrev=10
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    OUTPUT_VARIABLE RECOMPSTATION_BUILD_ID
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT RECOMPSTATION_BUILD_ID)
        set(RECOMPSTATION_BUILD_ID "unknown")
    endif()
endif()

qt_add_executable(RecompStationApp
    src/main.cpp
    src/core/ControllerManager.h
    src/core/ControllerManager.cpp
    src/core/UiSoundPlayer.h
    src/core/UiSoundPlayer.cpp
    data/ui_icons.qrc
    data/ui_sounds.qrc
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
        qml/SettingsButton.qml
        qml/UpdateDialog.qml
        qml/InputKeyChip.qml
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
    target_compile_options(recompstation_core PRIVATE -Wno-sfinae-incomplete)
    target_compile_options(RecompStationApp PRIVATE -Wno-sfinae-incomplete)
endif()

# Windows: a window program (no console), named RecompStation.exe, with the
# icon and version details Explorer shows.
if(WIN32)
    set(RECOMPSTATION_ICO "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/recompstation.ico")
    configure_file(packaging/windows/recompstation.rc.in
                   "${CMAKE_CURRENT_BINARY_DIR}/recompstation.rc" @ONLY)
    target_sources(RecompStationApp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/recompstation.rc")
    set_target_properties(RecompStationApp PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME RecompStation)
endif()

target_compile_definitions(RecompStationApp PRIVATE
    RECOMPSTATION_BUILD_ID="${RECOMPSTATION_BUILD_ID} ${CMAKE_BUILD_TYPE}"
    RECOMPSTATION_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(RecompStationApp PRIVATE
    recompstation_core
    Qt6::Quick Qt6::QuickControls2 Qt6::Svg
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

    # One executable per test file, linked against the shared core library.
    function(recompstation_add_test name)
        qt_add_executable(${name} ${ARGN})
        target_link_libraries(${name} PRIVATE recompstation_core Qt6::Test)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
           AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16")
            target_compile_options(${name} PRIVATE -Wno-sfinae-incomplete)
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    recompstation_add_test(ArchiveExtractorTests tests/ArchiveExtractorTest.cpp)
    recompstation_add_test(ControllerInputRouterTests tests/ControllerInputRouterTest.cpp)
    recompstation_add_test(SettingsManagerTests tests/SettingsManagerTest.cpp)
    recompstation_add_test(PlatformManagerTests tests/PlatformManagerTest.cpp)
    recompstation_add_test(GameImporterTests tests/GameImporterTest.cpp)
    recompstation_add_test(LauncherDownloadManagerTests tests/LauncherDownloadManagerTest.cpp)
    recompstation_add_test(GameLaunchManagerTests tests/GameLaunchManagerTest.cpp)
    recompstation_add_test(LaunchPreparerTests tests/LaunchPreparerTest.cpp)
    recompstation_add_test(LibraryScannerTests tests/LibraryScannerTest.cpp)
    recompstation_add_test(RomArchiveUnpackerTests tests/RomArchiveUnpackerTest.cpp)
    recompstation_add_test(DatabaseManagerTests tests/DatabaseManagerTest.cpp)
    recompstation_add_test(ArtworkManagerTests tests/ArtworkManagerTest.cpp)
    recompstation_add_test(LaunchBoxArtworkManagerTests tests/LaunchBoxArtworkManagerTest.cpp)
    recompstation_add_test(MetadataProviderTests tests/MetadataProviderTest.cpp)
    recompstation_add_test(TheGamesDbAllowanceTests tests/TheGamesDbAllowanceTest.cpp)
    recompstation_add_test(UiSoundGateTests tests/UiSoundGateTest.cpp)
    recompstation_add_test(LaunchBoxXmlParserTests tests/LaunchBoxXmlParserTest.cpp)
    recompstation_add_test(LaunchBoxCatalogMatcherTests tests/LaunchBoxCatalogMatcherTest.cpp)
    recompstation_add_test(LaunchBoxCatalogDatabaseTests tests/LaunchBoxCatalogDatabaseTest.cpp)
    recompstation_add_test(LaunchBoxArchiveExtractorTests tests/LaunchBoxArchiveExtractorTest.cpp)
    recompstation_add_test(N64RomHashTests tests/N64RomHashTest.cpp)
    recompstation_add_test(LibraryMetadataMatcherTests tests/LibraryMetadataMatcherTest.cpp)
    recompstation_add_test(ProjectCatalogTests tests/ProjectCatalogTest.cpp)
    recompstation_add_test(GitHubManagerTests tests/GitHubManagerTest.cpp tests/GitHubManagerTest.h)
    recompstation_add_test(AppUpdaterTests tests/AppUpdaterTest.cpp)
    recompstation_add_test(WineSupportTests tests/WineSupportTest.cpp)

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

# Installed layout, also used to build the AppImage and the portable zip
# (scripts/package.sh): the program, its desktop entry and its icons.
include(GNUInstallDirs)
set(RECOMPSTATION_APP_ID "io.github.MetalSJ13.RecompStation")
install(TARGETS RecompStationApp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/${RECOMPSTATION_APP_ID}.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
foreach(size 16 24 32 48 64 128 256)
    install(FILES assets/logo/png/recompstation-icon-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME ${RECOMPSTATION_APP_ID}.png)
endforeach()
install(FILES assets/logo/recompstation-mark.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME ${RECOMPSTATION_APP_ID}.svg)
