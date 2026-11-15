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

The tests are available through CTest when BUILD_TESTING is enabled. The launcher archive extractor requires libarchive (the `libarchive` package on Arch Linux).

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
- [x] Add a versioned, bundled JSON catalog with schema validation
- [x] Add the first curated project entry: Banjo: Recompiled for N64 Banjo-Kazooie
- [x] Match associated metadata titles to catalog aliases using normalized title and platform matching
- [x] Define x86-64 and ARM64 Linux release asset rules; explicitly exclude Windows and Flatpak archives from the initial native archive workflow
- [x] Record repository identity, ROM revision notes, package format, and expected executable path
- [x] Expose catalog lookup to the application/QML context
- [x] Show supported recompilation project matches in the local game library
- [x] Connect curated projects to GitHub latest-release discovery and present architecture-compatible Linux assets in the Launcher Discovery page
- [ ] Expand catalog entries after validating the initial end-to-end workflow

The catalog currently matches the associated metadata title and platform. It does not yet inspect ROM hashes/revisions; the Banjo: Recompiled entry documents its North American 1.0 requirement so we do not imply that title matching proves ROM compatibility.

#### Milestone 7: Launcher download, installation, and updates

Release discovery is connected to the curated project catalog. The Launcher Discovery page queries a project's latest GitHub release, displays assets matching the current CPU architecture and catalog filename rules, and downloads selected assets to staging. A successful download now triggers background extraction and installation without a separate user action.

The application data directory prepares `downloads/staging` for temporary downloads and installs projects under `launchers/<project-id>`. Existing per-platform directories are retained for compatibility. Directory creation and project-ID path validation have regression tests.

The asynchronous download manager streams selected HTTPS release assets into per-project staging folders using atomic temporary files. It reports progress and completion/failure to the Launcher Discovery page, supports cancellation, rejects unsafe project IDs and asset filenames, blocks insecure redirects, limits downloads to 8 GiB, checks the received size against GitHub's reported asset size, and guards against staging-directory symlinks.

Installation runs on a background thread and currently supports catalog-declared `tar.gz` and `tar.xz` packages. It validates the expected executable, writes a RecompStation ownership marker and manifest, refuses to overwrite an existing project installation, and promotes the completed directory into its permanent location. The source archive is deleted only after successful installation; failed installs retain it for retry. Installed launchers are rediscovered from their ownership marker and manifest after application restarts. The Launcher Discovery page offers a confirmation-protected uninstall that targets only validated RecompStation-managed launcher directories. Launcher manager regression coverage now creates a small tar.gz fixture, verifies installation and executable permissions, checks discovery from a fresh manager instance, confirms existing installations are not overwritten, and verifies that unowned directories and unrelated files survive uninstall.
- [x] Download selected release assets into a temporary staging area
- [x] Safely extract supported tar.gz and tar.xz archives while rejecting path traversal, links, and special files
- [x] Preserve required directory structure and supporting files during extraction
- [x] Apply executable permissions and validate the expected launcher executable
- [x] Install each project into a predictable, project-specific directory
- [x] Remove downloaded archives only after successful installation
- [x] Detect installed launchers across application restarts
- [x] Uninstall managed launcher files after user confirmation
- [ ] Track installed versions and support updates/rollback-safe replacement
- [ ] Add recovery and cleanup for abandoned temporary extraction directories

#### Optional release integrity verification
- [ ] Detect published SHA-256 checksum files/manifests and verify downloaded assets when available
- [ ] Detect detached signatures (for example, .sig files) and verify them using a trusted project-specific public key or documented trust mechanism
- [ ] Store verification status and communicate whether an asset was verified, unavailable for verification, or failed verification
- [ ] Do not block installation solely because a project publishes no checksum or signature
- [ ] Treat a published checksum/signature that fails verification as an integrity error and stop installation unless the user explicitly chooses an informed override

#### Milestone 8: Native game launching
- [x] Associate installed launchers with supported games in the Game Library
- [x] Add a Play Game action for supported games with a validated installed launcher
- [x] Launch the configured executable as a separate desktop process using its installation directory as the working directory
- [x] Track running process state and report launch failures and unexpected exits
- [x] Add regression coverage for launching a managed executable and tracking its process lifetime
- [ ] Add project-specific launch arguments and ROM/revision compatibility checks


### Build dependencies

- Qt 6.5 or newer
- libarchive (Arch Linux package: `libarchive`)

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
