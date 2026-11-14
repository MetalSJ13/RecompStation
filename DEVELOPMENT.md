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
- [ ] Verify a live TheGamesDB search using the local environment variable

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
- JSON parsing for game metadata and platform filtering
- Regression tests for missing keys, empty search terms, response parsing, and malformed JSON, plus MetadataManager configuration/error propagation

The API key is read from the process environment and is never written into source files or logs. MetadataManager now coordinates provider searches and exposes configuration, search state, results, and errors to QML. Search results are not yet persisted or displayed in a dedicated UI.

### Upcoming
- [ ] Metadata persistence and matching local platform IDs to TheGamesDB platform IDs
- [ ] Artwork retrieval and local caching
- [ ] Metadata result selection and matching to local games
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
