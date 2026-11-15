# Development Status

## Version 0.1.0

### Completed
- [x] Development environment setup
- [x] Qt 6 project configuration
- [x] QML interface
- [x] Platform directory creation
- [x] SQLite database initialization
- [x] Successful Linux application launch
- [x] Initial library scanner implementation
- [x] Supported game-file discovery for N64, PS1, GameCube, and Wii
- [x] SQLite game registration with duplicate-path protection
- [x] Basic game-library listing in QML

### In Progress
- [x] Build and run scanner, database, and metadata manager regression tests on Arch Linux
- [ ] Validate scanner behavior with representative game files and multi-file disc images
- [x] Verify live platform catalog retrieval and platform-filtered game search on Arch Linux

### Scanner regression coverage added
- Supported extensions across N64, PS1, GameCube, and Wii
- Recursive discovery and unsupported-file filtering
- Duplicate-scan protection
- PS1 cue-sheet companion handling, including mixed-case .cue extensions
- Single completion notification for scan-all
- Unknown-platform error reporting

The tests are available through CTest when BUILD_TESTING is enabled.

### Metadata provider foundation added
- Provider-independent GameMetadata data structure
- Abstract MetadataProvider interface
- Asynchronous TheGamesDB title search using RECOMPSTATION_TGDB_API_KEY
- JSON parsing for game metadata and the TheGamesDB platform catalog
- Runtime mapping from RecompStation's N64, PS1, GameCube, and Wii identifiers to TheGamesDB platform IDs
- Platform selector in Metadata Search; selected platform ID is passed to the existing platform-filtered game search
- Regression tests for platform catalog parsing and all four supported platform mappings, alongside search parsing and configuration/error handling

The API key is read from the process environment and is never written into source files or logs. MetadataManager now coordinates provider searches and exposes configuration, search state, results, and errors to QML. Search results are persisted through the ROM-to-metadata association feature described below.

### Metadata-to-ROM association
- [x] Add a local metadata table linked one-to-one with scanned game records
- [x] Expose local game IDs and existing metadata fields through the game query
- [x] Allow a user to select a local ROM and associate a TheGamesDB result
- [x] Allow an existing association to be corrected or replaced
- [x] Add database regression tests for association, replacement, and invalid game IDs

The association feature is merged into main and validated locally. Metadata is stored separately from the scanner's local game title and path, so provider titles do not overwrite local ROM records.

### Artwork retrieval and local caching
- [x] Parse TheGamesDB front-cover artwork URLs from search results
- [x] Persist artwork URLs with the metadata association
- [x] Download supported HTTPS artwork asynchronously
- [x] Cache artwork under the application data directory
- [x] Persist cached artwork paths on local game records
- [x] Display cached cover thumbnails in the game library
- [x] Add artwork parser, database, and input-validation tests

Merged into main and validated locally on Arch Linux; all four regression test suites passed and artwork association, download, and library display were confirmed.

### Upcoming development goals

#### Milestone 5: GitHub API foundation
- [x] Retrieve public repository information asynchronously
- [x] Retrieve latest published release information asynchronously
- [x] Parse release assets, including HTTPS download URLs, content types, and sizes
- [x] Report API, network, and response-parsing errors
- [x] Add regression tests for repository and release response parsing

#### Milestone 6: Supported project catalog and Linux launcher discovery
- [ ] Maintain a curated catalog of supported recompilation projects and their GitHub repositories
- [ ] Map supported games/platforms to the appropriate recompilation project
- [ ] Define supported Linux architectures and compatible release assets per project
- [ ] Support Linux-native AppImage, standalone executable, and supported archive releases (initially .tar.gz; consider .tar.xz as needed)
- [ ] Exclude Windows-only binaries; do not require Wine or Proton for the initial supported workflow
- [ ] Record each project's expected executable path, required supporting files, and launch arguments

#### Milestone 7: Launcher download, installation, and updates
- [ ] Download selected release assets into a temporary staging area
- [ ] Safely extract supported archives while preventing path traversal and writes outside the staging directory
- [ ] Preserve required directory structure and supporting files
- [ ] Apply executable permissions where appropriate and validate the installed launcher
- [ ] Install each project into a predictable, project-specific directory
- [ ] Remove downloaded archives and temporary files only after successful installation
- [ ] Track installed versions and support updates/rollback-safe replacement
- [ ] Clean up failed or incomplete installations without damaging an existing working installation

#### Optional release integrity verification
- [ ] Detect published SHA-256 checksum files/manifests and verify downloaded assets when available
- [ ] Detect detached signatures (for example, .sig files) and verify them using a trusted project-specific public key or documented trust mechanism
- [ ] Store verification status and communicate whether an asset was verified, unavailable for verification, or failed verification
- [ ] Do not block installation solely because a project publishes no checksum or signature
- [ ] Treat a published checksum/signature that fails verification as an integrity error and stop installation unless the user explicitly chooses an informed override

#### Milestone 8: Native game launching
- [ ] Associate installed launchers with supported games
- [ ] Launch the configured executable with project-specific arguments
- [ ] Track process state and report launch/runtime errors


### Build script
From the repository root, run:

```bash
./scripts/build.sh
```

Run the build and scanner regression tests together:

```bash
./scripts/build.sh --test
```

Choose a build configuration with `./scripts/build.sh Release` or `./scripts/build.sh RelWithDebInfo`. The script enables tests by default and reuses the generator already configured in the build directory.
