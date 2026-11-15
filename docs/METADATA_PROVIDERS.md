# Metadata Provider Architecture

## Purpose

RecompStation should present one unified game-library entry while allowing metadata and artwork to come from different providers. This is an internal architecture decision, not a requirement to expose separate metadata and artwork workflows in the UI.

TheGamesDB remains the existing provider and must continue to work when no other provider is configured. ScreenScraper is a planned optional provider, not a mandatory dependency.

## Core concepts

### 1. Stable library identity

A library game is identified by RecompStation's own database record and internal platform key (for example, `N64`, `PS1`, `GameCube`, or `Wii`). Provider-specific game IDs and platform IDs must never be treated as universal identifiers.

Each provider adapter is responsible for translating RecompStation's internal platform key into that provider's platform identifier.

### 2. Provider adapters

Each provider implements a common contract for the capabilities it supports:

- Report a stable provider identifier and whether it is configured.
- Search or identify games using the available title, platform, and optional ROM hashes.
- Return normalized game metadata with provider provenance.
- Retrieve artwork candidates independently from descriptive metadata.
- Report provider-specific errors, limits, and capabilities without leaking those details into library-matching logic.

Provider-specific response formats, IDs, platform catalogs, authentication, and rate limits stay inside the adapter. Shared matching and library code should not depend on TheGamesDB-specific types.

### 3. Independent resources, unified presentation

Metadata and artwork are separate retrieval resources associated with the same library game.

- Metadata includes title, description, release date, developer, publisher, genre, rating, and other descriptive fields.
- Artwork includes an image URL or provider asset reference, image type/role, dimensions where available, and its source provider.

A successful metadata match remains successful if artwork is absent or its download fails. Missing artwork can be retried or sourced from another provider without repeating or discarding valid metadata. Conversely, artwork availability must not be treated as proof that game identification is correct.

The UI continues to show one game entry and one cohesive details experience. Provider names and retry statuses are primarily diagnostic/configuration information, not mandatory clutter in the library.

### 4. Provenance and persistence

Persist enough provenance to understand and refresh each resource independently:

- Provider identifier and provider game ID for the accepted metadata record.
- Provider identifier and provider game ID (or asset ID, when available) for the selected artwork.
- Artwork URL/reference and local cached path.
- Retrieval/verification status and last-attempt information for metadata and artwork separately.

Provider IDs must be scoped by provider. The same numeric ID from two services is not the same game record.

Database changes should be additive and migration-safe. Existing TheGamesDB associations and cached artwork must remain valid. Do not overwrite good metadata or artwork merely because another provider returned a candidate.

## Matching and fallback policy

1. Identify the local game and its RecompStation platform.
2. Reuse previously accepted metadata and provider identity when available; do not re-run title matching just to retry artwork.
3. Query configured providers according to a deterministic priority order.
4. Prefer strong identification evidence: exact provider/platform match, normalized title, and where supported ROM hashes (CRC/MD5/SHA-1). Hashes must be computed from the actual ROM bytes and tied to the correct platform/region context.
5. Keep ambiguous or low-confidence matches unresolved. A candidate from another platform must never be accepted.
6. Once identity is established, retrieve metadata and artwork independently.
7. If artwork is missing or fails validation/download, try another configured provider for artwork without invalidating the metadata association.
8. Cache successful results and respect provider quotas, throttling, and retry/backoff behavior.
9. Record resource-specific outcomes so the UI and logs can distinguish unmatched games, metadata failures, missing artwork, and artwork download failures.

Provider fallback must be bounded and deterministic. Avoid querying every provider repeatedly when a result is already cached or a provider has recently failed for the same resource.

## Configuration and security

- TheGamesDB remains usable by itself.
- ScreenScraper is optional and only participates when the user configures it.
- Provider credentials are local user configuration and must never be committed, logged, included in diagnostics, or exposed to QML unnecessarily.
- Provide a connection/credential test before enabling a provider.
- Respect each service's documented authentication, usage limits, and terms.
- Do not make account credentials or a provider outage prevent browsing or launching games already in the library.

The exact credential-storage mechanism should be selected after reviewing the application's existing settings infrastructure and the target desktop environments. Prefer an OS-backed secret store when a suitable cross-platform solution is available; do not silently fall back to committing secrets or writing them into project files.

## Implementation sequence

1. Refactor the existing TheGamesDB integration behind a provider-neutral contract while preserving current behavior and tests.
2. Move provider-specific platform mapping and response types into provider adapters.
3. Separate metadata matching/persistence from artwork candidate selection, download, and retry state.
4. Add provider registry/coordination and deterministic fallback policy.
5. Add migration-safe resource provenance and independent status tracking.
6. Implement ScreenScraper as an optional adapter after credentials and API behavior are verified.
7. Add hash-based identification where supported, with conservative title/platform fallback.
8. Add tests for provider ordering, provider failure, ambiguous matches, cross-provider fallback, independent artwork retry, persistence migration, and secret redaction.

## Non-goals

- Do not make users manually select providers for every game.
- Do not expose two separate library entries for metadata and artwork.
- Do not replace valid metadata or artwork automatically without a defined quality/priority policy.
- Do not make ScreenScraper mandatory.
- Do not treat a metadata-provider match as proof that a ROM is compatible with a recompilation project.
