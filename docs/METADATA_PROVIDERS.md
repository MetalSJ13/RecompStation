# Metadata Providers

Last reviewed: 2026-11-25. How RecompStation finds titles, descriptions, covers and screenshots for the games in the library, and the rules that keep it reliable.

## Purpose

Each game in the library is one entry with one details page. Its metadata (title, overview, release date, rating) and its artwork (a cover and up to three gameplay screenshots) can come from either provider; the player never chooses a provider per game, and matching happens in the background.

## Providers

### LaunchBox (offline catalog, asked first by default)

- The LaunchBox games database is downloaded once as an archive (from first-time setup or Settings › Metadata), extracted and indexed into a local SQLite catalog (`LaunchBoxCatalogManager`, `LaunchBoxXmlParser`, `LaunchBoxCatalogDatabase`). Only RecompStation's systems are indexed. Update catalog refreshes it.
- Matching (`LaunchBoxCatalogMatcher`): the local file name is looked up in LaunchBox's `Files.xml` first; if that names a game, its record in `Metadata.xml` is used. Otherwise a conservative title match on the same system is tried. Only explicit system mappings are accepted; LaunchBox's platform aliases are not treated as interchangeable.
- Artwork (`LaunchBoxArtworkManager`): one `Box - Front` cover (North American artwork preferred) and up to three `Screenshot - Gameplay` images, only for games in the user's library and only through a saved LaunchBox game ID. Images come from `images.launchbox-app.com`, requested one at a time with a 300 ms pause after every five.
- A catalog downloaded before a system was supported has none of that system's games; the LaunchBox card names those systems and points to Update catalog.

### TheGamesDB (online, the user's own key)

- Each user enters their own API key in Settings › Metadata; it is stored in an owner-only file (the `THEGAMESDB_API_KEY` environment variable still wins). RecompStation never ships a key: TheGamesDB counts allowances per key and advised against bundling one (2026-11-24).
- `TheGamesDbClient` searches by title and system. The platform list is reused for 30 days; screenshots are looked up 20 games per request, and games without any are not asked again for 30 days.
- `TheGamesDbAllowance` protects the monthly allowance: automatic lookups pause at 50 or fewer requests left, and a 403 pauses them for 6 hours. Test in Settings shows the connection and requests left.

## Matching pipeline

`LibraryMetadataMatcher` and `MetadataManager`:

1. The scanner or the filesystem watcher registers a new game first.
2. Games that already have saved metadata, or a finished lookup, are skipped. Provider errors are recorded and retried after an hour.
3. The file name is turned into a search title by removing region tags (one or several, such as "(USA)" or "(Japan, USA)") and language tags ("(En,Fr,De)"), and by moving a trailing article to the front ("Legend of Zelda, The - …" → "The Legend of Zelda …"). Revision tags ("(Rev A)", "(v1.1)") are also ignored when comparing results.
4. Providers are asked in the order chosen in Settings (`metadataProviderOrder`, LaunchBox first by default). When TheGamesDB fails, LaunchBox is tried.
5. A result is accepted only when exactly one candidate clearly matches on the same system. Ambiguous games stay unmatched rather than getting the wrong cover.
6. The accepted record is saved with its provider and provider game ID (`game_metadata`); provider IDs are always scoped by provider. Artwork is downloaded and cached separately, so a missing image never undoes a good match, and retrying artwork never re-runs matching.
7. Enabling LaunchBox or updating its catalog re-matches games that are still unmatched.

## Provider contract

`MetadataProvider` (`src/metadata/MetadataProvider.h`) is the shared interface: `providerId()`, `isConfigured()`, `searchGames(title, platform)`, `fetchPlatforms()` and an optional cached platform list, with signals for results, failures, platforms and the remaining allowance. Provider-specific IDs, response formats, system mappings (`MetadataPlatformMapping.h`), authentication and limits stay inside each provider.

## Rules

- Never attach a low-confidence or cross-system match.
- Never download artwork for catalog-only games, or infer artwork from a title alone.
- Never replace good metadata or artwork just because another provider returned a candidate.
- Credentials are never committed, logged, put in diagnostics or exposed to QML beyond what Settings needs.
- A provider outage or missing key never stops browsing or playing games already in the library.
- A metadata match is not proof that a ROM suits a recompilation project; the launch recipes check that separately.

## Not planned or not yet done

- ScreenScraper and other artwork providers are not planned; artwork comes from LaunchBox and TheGamesDB.
- Fix match: a Game Details action to search either provider and pick the right game (planned).
- Bounded retry and backoff for HTTP 429 and 5xx responses (planned).
- Matching by ROM hash: N64 ROM hashes are computed for launch checks (`N64RomHash`), not yet used for metadata.
