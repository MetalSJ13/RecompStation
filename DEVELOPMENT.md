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
- [ ] Build and run scanner and metadata manager regression tests on Arch Linux
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

The API key is read from the process environment and is never written into source files or logs. MetadataManager now coordinates provider searches and exposes configuration, search state, results, and errors to QML. Search results are displayed in the Metadata Search page but are not yet persisted or associated with individual local ROM records.

### Upcoming
- [ ] Persist metadata and platform catalog mappings locally
- [ ] Artwork retrieval and local caching
- [ ] Metadata result selection and association with individual local ROM records
- [ ] GitHub integration
- [ ] Launcher installation
- [ ] Game launching


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
