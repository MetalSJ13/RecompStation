#include "GameImporter.h"
#include "Logging.h"
#include "PlatformManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QRegularExpression>
#include <QSet>
#include <QStorageInfo>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <archive.h>
#include <archive_entry.h>

namespace {
constexpr qint64 CopyChunk = 4 * 1024 * 1024;
const QStringList N64Extensions{QStringLiteral("z64"), QStringLiteral("n64"), QStringLiteral("v64")};
// Cartridge ROMs recognised inside .zip/.7z archives, and their systems.
const QStringList CartridgeExtensions{
    QStringLiteral("z64"), QStringLiteral("n64"), QStringLiteral("v64"), QStringLiteral("gb"),
    QStringLiteral("gbc"), QStringLiteral("gba"), QStringLiteral("sfc"), QStringLiteral("smc")};
// Enough of a Game Boy ROM to read its header.
constexpr qint64 CartridgeHeadSize = 0x150;

quint8 byteAt(const QByteArray &data, qint64 offset)
{
    return offset < data.size() ? quint8(data.at(offset)) : 0;
}

// Platform of a cartridge ROM from its extension and first bytes. Each of
// these extensions belongs to one system, except that Game Boy and Game Boy
// Color ROMs are told apart by the CGB flag at 0x143 of their header
// (0xC0: Game Boy Color only; 0x80: runs on both, so the extension decides).
// `sure` is false only for a Game Boy file without the Nintendo logo at 0x104.
QString cartridgePlatform(const QByteArray &head, const QString &extension, bool *sure)
{
    *sure = true;
    if (N64Extensions.contains(extension))
        return QStringLiteral("N64");
    if (extension == QStringLiteral("gba"))
        return QStringLiteral("GBA");
    if (extension == QStringLiteral("sfc") || extension == QStringLiteral("smc"))
        return QStringLiteral("SNES");
    if (extension == QStringLiteral("gb") || extension == QStringLiteral("gbc")) {
        const bool logo = head.mid(0x104, 4) == QByteArray::fromHex("ceed6666");
        *sure = logo;
        const quint8 cgb = byteAt(head, 0x143);
        if (logo && cgb == 0xC0)
            return QStringLiteral("GBC");
        if (logo && cgb != 0x80)
            return QStringLiteral("GB");
        return extension == QStringLiteral("gbc") ? QStringLiteral("GBC") : QStringLiteral("GB");
    }
    *sure = false;
    return {};
}

// Which PlayStation a disc image belongs to: 1, 2 or 0 when it isn't one.
// Both use ISO 9660 with "PLAYSTATION" as the system identifier, cooked
// (2048-byte sectors) or raw (2352-byte sectors); SYSTEM.CNF in the root
// directory names the boot file with "BOOT2" on PS2 and "BOOT" on PS1.
int playStationGeneration(QFile &file)
{
    struct Layout { qint64 sectorSize; qint64 dataOffset; };
    for (const Layout layout : {Layout{2048, 0}, Layout{2352, 24}, Layout{2352, 16}}) {
        const auto sector = [&](qint64 lba) { return lba * layout.sectorSize + layout.dataOffset; };
        const qint64 descriptor = sector(16);
        if (!hasBytes(file, descriptor + 1, QByteArray("CD001"))
            || !readAt(file, descriptor + 8, 32).startsWith("PLAYSTATION"))
            continue;

        const QByteArray rootRecord = readAt(file, descriptor + 156, 34);
        const auto u32 = [](const QByteArray &data, int offset) {
            return quint32(byteAt(data, offset)) | quint32(byteAt(data, offset + 1)) << 8
                | quint32(byteAt(data, offset + 2)) << 16 | quint32(byteAt(data, offset + 3)) << 24;
        };
        const qint64 rootLba = u32(rootRecord, 2);
        const qint64 rootSize = qMin<qint64>(u32(rootRecord, 10), 2048 * 4);
        QByteArray root;
        for (qint64 read = 0; read < rootSize; read += 2048)
            root += readAt(file, sector(rootLba + read / 2048), 2048);

        // Walk the directory records looking for SYSTEM.CNF.
        for (int pos = 0; pos + 33 < root.size();) {
            const int length = byteAt(root, pos);
            if (length == 0) {
                pos = (pos / 2048 + 1) * 2048;  // records never cross a sector
                continue;
            }
            const int nameLength = byteAt(root, pos + 32);
            const QByteArray name = root.mid(pos + 33, nameLength).toUpper();
            if (name == "SYSTEM.CNF;1" || name == "SYSTEM.CNF") {
                const QByteArray config = readAt(file, sector(u32(root, pos + 2)), 512).toUpper();
                if (config.contains("BOOT2"))
                    return 2;
                return 1;
            }
            pos += length;
        }
        return 1;  // PlayStation disc without a readable SYSTEM.CNF
    }
    return 0;
}

// The kind of disc a CHD holds, from its metadata (stored uncompressed):
// "DVD" for DVD images, "CD" for CD images, empty if unknown.
QString chdMediaType(QFile &file)
{
    const QByteArray header = readAt(file, 0, 64);
    if (!header.startsWith("MComprHD"))
        return {};
    const auto be = [&header](int offset, int bytes) {
        quint64 value = 0;
        for (int i = 0; i < bytes; ++i)
            value = value << 8 | byteAt(header, offset + i);
        return value;
    };
    const quint64 version = be(12, 4);
    quint64 offset = version >= 5 ? be(48, 8) : (version >= 3 ? be(36, 8) : 0);
    for (int entries = 0; offset != 0 && entries < 32; ++entries) {
        const QByteArray entry = readAt(file, qint64(offset), 16);
        if (entry.size() < 16)
            break;
        const QByteArray tag = entry.left(4);
        if (tag == "DVD ")
            return QStringLiteral("DVD");
        if (tag == "CHT2" || tag == "CHTR" || tag == "CHCD")
            return QStringLiteral("CD");
        quint64 next = 0;
        for (int i = 8; i < 16; ++i)
            next = next << 8 | byteAt(entry, i);
        offset = next;
    }
    return {};
}

bool isRawCdImage(QFile &file)
{
    return readAt(file, 0, 12) == QByteArray::fromHex("00ffffffffffffffffffff00");
}

// Files listed by a .cue sheet that exist next to it.
QStringList cueTracks(const QString &cuePath)
{
    QStringList tracks;
    QFile cue(cuePath);
    if (!cue.open(QIODevice::ReadOnly | QIODevice::Text) || cue.size() > 1024 * 1024)
        return tracks;
    static const QRegularExpression fileLine(
        QStringLiteral(R"(^\s*FILE\s+"([^"]+)\")"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bareFileLine(
        QStringLiteral(R"(^\s*FILE\s+(\S+)\s)"), QRegularExpression::CaseInsensitiveOption);
    const QDir dir = QFileInfo(cuePath).dir();
    while (!cue.atEnd()) {
        const QString line = QString::fromUtf8(cue.readLine());
        QRegularExpressionMatch match = fileLine.match(line);
        if (!match.hasMatch())
            match = bareFileLine.match(line);
        if (!match.hasMatch())
            continue;
        const QString name = QFileInfo(match.captured(1)).fileName();  // no paths
        const QString path = dir.filePath(name);
        if (QFileInfo(path).isFile() && !tracks.contains(path))
            tracks.append(path);
    }
    return tracks;
}

struct ArchiveRom { QString name; qint64 size = 0; QString platform; bool sure = false; };

QList<ArchiveRom> romsInArchive(const QString &archivePath)
{
    QList<ArchiveRom> roms;
    archive *reader = archive_read_new();
    archive_read_support_filter_all(reader);
    archive_read_support_format_zip(reader);
    archive_read_support_format_7zip(reader);
    if (archive_read_open_filename(reader, QFile::encodeName(archivePath).constData(), 64 * 1024) != ARCHIVE_OK) {
        archive_read_free(reader);
        return roms;
    }
    archive_entry *entry = nullptr;
    int scanned = 0;
    while (archive_read_next_header(reader, &entry) == ARCHIVE_OK && ++scanned < 200) {
        if (archive_entry_filetype(entry) != AE_IFREG)
            continue;
        const QString name = QFileInfo(QString::fromUtf8(archive_entry_pathname(entry))).fileName();
        const QString extension = QFileInfo(name).suffix().toLower();
        if (!CartridgeExtensions.contains(extension)) {
            archive_read_data_skip(reader);
            continue;
        }
        // Read the start of the ROM to check its header.
        QByteArray head;
        char buffer[4096];
        while (head.size() < CartridgeHeadSize) {
            const la_ssize_t count = archive_read_data(reader, buffer, sizeof(buffer));
            if (count <= 0)
                break;
            head.append(buffer, int(count));
        }
        bool sure = false;
        const QString platform = cartridgePlatform(head, extension, &sure);
        roms.append({name, archive_entry_size(entry), platform, sure});
        archive_read_data_skip(reader);
    }
    archive_read_free(reader);
    return roms;
}

QVariantMap candidate(const QString &path, const QStringList &files, const QString &name,
                      const QString &platform, bool sure, qint64 size,
                      const QString &archiveEntry = {})
{
    return QVariantMap{
        {QStringLiteral("path"), path},
        {QStringLiteral("files"), files},
        {QStringLiteral("name"), name},
        {QStringLiteral("platform"), platform},
        {QStringLiteral("sure"), sure},
        {QStringLiteral("selected"), sure && !platform.isEmpty()},
        {QStringLiteral("size"), size},
        {QStringLiteral("archiveEntry"), archiveEntry},
        {QStringLiteral("exists"), false},
    };
}
}

GameImporter::GameImporter(PlatformManager *platformManager, QObject *parent)
    : QObject(parent),
      m_platformManager(platformManager)
{
    connect(&m_scanWatcher, &QFutureWatcher<QVariantList>::finished, this, [this]() {
        m_candidates = m_scanWatcher.result();
        markExisting();
        emit candidatesChanged();
        emit scanningChanged();
        setStatus(m_candidates.isEmpty()
            ? (m_sources.isEmpty() ? QString() : QStringLiteral("No games found in these locations."))
            : QStringLiteral("%1 game(s) found.").arg(m_candidates.size()));
        if (m_rescanRequested) {
            m_rescanRequested = false;
            startScan();
        }
    });
    connect(&m_importWatcher, &QFutureWatcher<ImportResult>::finished, this, [this]() {
        const ImportResult result = m_importWatcher.result();
        // Imported entries leave the list; the rest stay for another try.
        QVariantList remaining;
        for (const QVariant &entry : m_candidates) {
            const QVariantMap item = entry.toMap();
            const QString key = item.value(QStringLiteral("path")).toString() + QLatin1Char('|')
                + item.value(QStringLiteral("archiveEntry")).toString();
            if (!result.importedPaths.contains(key))
                remaining.append(entry);
        }
        m_candidates = remaining;
        markExisting();
        m_currentName.clear();
        emit candidatesChanged();
        emit progressChanged();
        emit importingChanged();
        setStatus(QStringLiteral("Added %1 game(s)%2%3.")
                      .arg(result.imported)
                      .arg(result.skipped > 0 ? QStringLiteral(", %1 already in your library").arg(result.skipped) : QString())
                      .arg(result.failed > 0 ? QStringLiteral(", %1 failed (see the runtime log)").arg(result.failed) : QString()));
        qCInfo(lcLibrary) << "Game import finished. Imported:" << result.imported
                          << "skipped:" << result.skipped << "failed:" << result.failed;
        emit importFinished(result.imported, result.skipped, result.failed);
    });
}

GameImporter::~GameImporter()
{
    if (m_cancel)
        m_cancel->store(true);
    if (m_scanWatcher.isRunning())
        m_scanWatcher.waitForFinished();
    if (m_importWatcher.isRunning())
        m_importWatcher.waitForFinished();
}

QStringList GameImporter::sources() const { return m_sources; }
bool GameImporter::scanning() const { return m_scanWatcher.isRunning(); }
bool GameImporter::importing() const { return m_importWatcher.isRunning(); }
QVariantList GameImporter::candidates() const { return m_candidates; }
int GameImporter::completedCount() const { return m_completed.load(); }
int GameImporter::totalCount() const { return m_total; }
qint64 GameImporter::bytesDone() const { return m_bytesDone.load(); }
qint64 GameImporter::bytesTotal() const { return m_bytesTotal; }
QString GameImporter::currentName() const { return m_currentName; }
QString GameImporter::status() const { return m_status; }

QString GameImporter::localPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    return QDir::cleanPath(path);
}

void GameImporter::addSource(const QString &pathOrUrl)
{
    const QString path = localPath(pathOrUrl);
    if (path.isEmpty() || !QFileInfo::exists(path) || m_sources.contains(path))
        return;
    m_sources.append(path);
    emit sourcesChanged();
    startScan();
}

void GameImporter::removeSource(const QString &path)
{
    if (!m_sources.removeOne(path))
        return;
    emit sourcesChanged();
    startScan();
}

void GameImporter::setSources(const QVariantList &pathsOrUrls)
{
    m_sources.clear();
    for (const QVariant &value : pathsOrUrls) {
        const QString path = localPath(value.toString());
        if (!path.isEmpty() && QFileInfo::exists(path) && !m_sources.contains(path))
            m_sources.append(path);
    }
    emit sourcesChanged();
    startScan();
}

void GameImporter::clear()
{
    if (importing())
        return;
    m_sources.clear();
    m_candidates.clear();
    emit sourcesChanged();
    emit candidatesChanged();
    setStatus({});
}

void GameImporter::startScan()
{
    if (m_scanWatcher.isRunning()) {
        m_rescanRequested = true;
        if (m_cancel)
            m_cancel->store(true);
        return;
    }
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = m_cancel;
    const QStringList sources = m_sources;
    const QString libraryRoot = m_platformManager ? m_platformManager->libraryDirectory() : QString();
    setStatus(sources.isEmpty() ? QString() : QStringLiteral("Looking for games…"));
    m_scanWatcher.setFuture(QtConcurrent::run([sources, libraryRoot, cancel]() {
        return findCandidates(sources, libraryRoot, cancel.get());
    }));
    emit scanningChanged();
}

void GameImporter::markExisting()
{
    // A game whose files are already in the matching platform folder is shown
    // as "already in your library" and is not selected.
    for (int i = 0; i < m_candidates.size(); ++i) {
        QVariantMap item = m_candidates.at(i).toMap();
        const QString platform = item.value(QStringLiteral("platform")).toString();
        const QString directory = m_platformManager && !platform.isEmpty()
            ? m_platformManager->platformDirectory(platform) : QString();
        const QString firstName = item.value(QStringLiteral("archiveEntry")).toString().isEmpty()
            ? QFileInfo(item.value(QStringLiteral("files")).toStringList().value(0)).fileName()
            : item.value(QStringLiteral("archiveEntry")).toString();
        const bool exists = !directory.isEmpty() && QFileInfo::exists(QDir(directory).filePath(firstName));
        item.insert(QStringLiteral("exists"), exists);
        if (exists)
            item.insert(QStringLiteral("selected"), false);
        m_candidates[i] = item;
    }
}

void GameImporter::setCandidateSelected(int index, bool selected)
{
    if (index < 0 || index >= m_candidates.size() || importing())
        return;
    QVariantMap item = m_candidates.at(index).toMap();
    if (selected && (item.value(QStringLiteral("platform")).toString().isEmpty()
                     || item.value(QStringLiteral("exists")).toBool()))
        return;
    item.insert(QStringLiteral("selected"), selected);
    m_candidates[index] = item;
    emit candidatesChanged();
}

void GameImporter::setCandidatePlatform(int index, const QString &platform)
{
    if (index < 0 || index >= m_candidates.size() || importing())
        return;
    if (!platform.isEmpty() && m_platformManager && !m_platformManager->platforms().contains(platform))
        return;
    QVariantMap item = m_candidates.at(index).toMap();
    item.insert(QStringLiteral("platform"), platform);
    item.insert(QStringLiteral("selected"), !platform.isEmpty());
    m_candidates[index] = item;
    markExisting();
    emit candidatesChanged();
}

int GameImporter::selectedCount() const
{
    int count = 0;
    for (const QVariant &entry : m_candidates)
        count += entry.toMap().value(QStringLiteral("selected")).toBool() ? 1 : 0;
    return count;
}

qint64 GameImporter::selectedBytes() const
{
    qint64 bytes = 0;
    for (const QVariant &entry : m_candidates) {
        const QVariantMap item = entry.toMap();
        if (item.value(QStringLiteral("selected")).toBool())
            bytes += item.value(QStringLiteral("size")).toLongLong();
    }
    return bytes;
}

qint64 GameImporter::libraryFreeBytes() const
{
    if (!m_platformManager || m_platformManager->libraryDirectory().isEmpty())
        return -1;
    return QStorageInfo(m_platformManager->libraryDirectory()).bytesAvailable();
}

void GameImporter::cancel()
{
    if (m_cancel)
        m_cancel->store(true);
}

void GameImporter::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

QString GameImporter::detectPlatform(const QString &filePath, bool *sure)
{
    bool certain = true;
    QString platform;
    const QString extension = QFileInfo(filePath).suffix().toLower();
    QFile file(filePath);
    const bool readable = file.open(QIODevice::ReadOnly);

    if (CartridgeExtensions.contains(extension)) {
        const QByteArray head = readable ? file.read(CartridgeHeadSize) : QByteArray();
        platform = cartridgePlatform(head, extension, &certain);
    } else if (extension == QStringLiteral("pbp")) {
        platform = QStringLiteral("PS1");
    } else if (extension == QStringLiteral("cue")) {
        // Check the first data track: PS2 also shipped some games on CD.
        platform = QStringLiteral("PS1");
        const QStringList tracks = cueTracks(filePath);
        if (!tracks.isEmpty()) {
            QFile track(tracks.first());
            if (track.open(QIODevice::ReadOnly) && playStationGeneration(track) == 2)
                platform = QStringLiteral("PS2");
        }
    } else if (extension == QStringLiteral("chd")) {
        // DVD images are PS2; CD images are almost always PS1.
        const QString media = readable ? chdMediaType(file) : QString();
        platform = media == QStringLiteral("DVD") ? QStringLiteral("PS2") : QStringLiteral("PS1");
        certain = !media.isEmpty();
    } else if (extension == QStringLiteral("gcm") || extension == QStringLiteral("ciso")) {
        platform = QStringLiteral("GameCube");
    } else if (extension == QStringLiteral("wbfs")) {
        platform = QStringLiteral("Wii");
    } else if (extension == QStringLiteral("wud") || extension == QStringLiteral("wux")
               || extension == QStringLiteral("wua")) {
        platform = QStringLiteral("Wii U");
    } else if (extension == QStringLiteral("xiso")) {
        platform = QStringLiteral("Xbox");
    } else if (extension == QStringLiteral("rvz") && readable) {
        // RVZ/WIA keep a copy of the disc header at 0x58.
        if (hasBytes(file, 0, QByteArray("RVZ\x01", 4)) || hasBytes(file, 0, QByteArray("WIA\x01", 4)))
            platform = nintendoDiscPlatform(file, 0x58);
        certain = !platform.isEmpty();
    } else if (extension == QStringLiteral("iso") && readable) {
        platform = nintendoDiscPlatform(file, 0);
        if (platform.isEmpty())
            platform = xboxDiscPlatform(file);
        if (platform.isEmpty()) {
            const int generation = playStationGeneration(file);
            if (generation == 2)
                platform = QStringLiteral("PS2");
            else if (generation == 1)
                platform = QStringLiteral("PS1");
        }
        certain = !platform.isEmpty();
    } else if ((extension == QStringLiteral("bin") || extension == QStringLiteral("img")) && readable) {
        // A loose CD image without a .cue: most likely PS1, but not certain.
        const int generation = playStationGeneration(file);
        if (generation == 2)
            platform = QStringLiteral("PS2");
        else if (generation == 1 || isRawCdImage(file))
            platform = QStringLiteral("PS1");
        certain = false;
    } else {
        certain = false;
    }
    if (sure)
        *sure = certain && !platform.isEmpty();
    return platform;
}

QVariantList GameImporter::findCandidates(const QStringList &paths, const QString &excludedRoot,
                                          const std::atomic_bool *cancelled)
{
    static const QSet<QString> gameExtensions{
        QStringLiteral("z64"), QStringLiteral("n64"), QStringLiteral("v64"), QStringLiteral("zip"),
        QStringLiteral("7z"), QStringLiteral("cue"), QStringLiteral("chd"), QStringLiteral("pbp"),
        QStringLiteral("iso"), QStringLiteral("bin"), QStringLiteral("img"), QStringLiteral("gcm"),
        QStringLiteral("rvz"), QStringLiteral("ciso"), QStringLiteral("wbfs"), QStringLiteral("wud"),
        QStringLiteral("wux"), QStringLiteral("wua"), QStringLiteral("xiso"), QStringLiteral("gb"),
        QStringLiteral("gbc"), QStringLiteral("gba"), QStringLiteral("sfc"), QStringLiteral("smc")};
    const QString excluded = excludedRoot.isEmpty() ? QString()
        : QFileInfo(excludedRoot).canonicalFilePath() + QLatin1Char('/');

    // Collect files first, so .cue sheets can claim their tracks.
    QStringList files;
    int visited = 0;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isFile()) {
            files.append(info.absoluteFilePath());
            continue;
        }
        if (!info.isDir())
            continue;
        QDirIterator it(info.absoluteFilePath(), QDir::Files | QDir::NoSymLinks,
                        QDirIterator::Subdirectories);
        while (it.hasNext() && ++visited < 200000) {
            if (cancelled && cancelled->load())
                return {};
            it.next();
            const QFileInfo fileInfo = it.fileInfo();
            if (fileInfo.fileName().startsWith(QLatin1Char('.'))
                || fileInfo.absoluteFilePath().contains(QStringLiteral("/.")))
                continue;
            if (gameExtensions.contains(fileInfo.suffix().toLower()))
                files.append(fileInfo.absoluteFilePath());
        }
    }
    files.removeDuplicates();

    QSet<QString> claimedTracks;
    for (const QString &file : files) {
        if (QFileInfo(file).suffix().compare(QStringLiteral("cue"), Qt::CaseInsensitive) == 0) {
            for (const QString &track : cueTracks(file))
                claimedTracks.insert(QFileInfo(track).absoluteFilePath());
        }
    }

    QVariantList result;
    for (const QString &file : files) {
        if (cancelled && cancelled->load())
            return {};
        const QFileInfo info(file);
        if (!excluded.isEmpty() && (info.canonicalFilePath() + QLatin1Char('/')).startsWith(excluded))
            continue;  // already inside the library
        if (claimedTracks.contains(info.absoluteFilePath()))
            continue;  // imported together with its .cue
        const QString extension = info.suffix().toLower();

        if (extension == QStringLiteral("zip") || extension == QStringLiteral("7z")) {
            for (const ArchiveRom &rom : romsInArchive(file))
                result.append(candidate(file, {file}, rom.name, rom.platform, rom.sure,
                                        rom.size, rom.name));
            continue;
        }
        bool sure = false;
        const QString platform = detectPlatform(file, &sure);
        QStringList groupFiles{info.absoluteFilePath()};
        qint64 size = info.size();
        QString name = info.fileName();
        if (extension == QStringLiteral("cue")) {
            const QStringList tracks = cueTracks(file);
            for (const QString &track : tracks) {
                groupFiles.append(QFileInfo(track).absoluteFilePath());
                size += QFileInfo(track).size();
            }
            if (!tracks.isEmpty())
                name += QStringLiteral("  + %1 track%2").arg(tracks.size()).arg(tracks.size() == 1 ? "" : "s");
        }
        // Loose files of unknown type are not games we can place; skip them
        // unless their extension is one a supported system uses.
        if (platform.isEmpty() && extension != QStringLiteral("iso")
            && extension != QStringLiteral("rvz"))
            continue;
        result.append(candidate(info.absoluteFilePath(), groupFiles, name, platform, sure, size));
    }
    return result;
}

void GameImporter::importSelected(bool move)
{
    if (importing() || scanning() || !m_platformManager
        || m_platformManager->libraryDirectory().isEmpty())
        return;
    QVariantList items;
    m_bytesTotal = 0;
    for (const QVariant &entry : m_candidates) {
        const QVariantMap item = entry.toMap();
        if (!item.value(QStringLiteral("selected")).toBool())
            continue;
        QVariantMap job = item;
        job.insert(QStringLiteral("destination"),
                   m_platformManager->platformDirectory(item.value(QStringLiteral("platform")).toString()));
        items.append(job);
        m_bytesTotal += item.value(QStringLiteral("size")).toLongLong();
    }
    if (items.isEmpty())
        return;
    m_total = items.size();
    m_completed = 0;
    m_bytesDone = 0;
    m_cancel = std::make_shared<std::atomic_bool>(false);
    setStatus(move ? QStringLiteral("Moving games into your library…")
                   : QStringLiteral("Copying games into your library…"));
    qCInfo(lcLibrary) << "Importing" << items.size() << "game(s)," << m_bytesTotal << "bytes, move:" << move;
    m_importWatcher.setFuture(QtConcurrent::run([this, items, move]() { return runImport(items, move); }));
    emit importingChanged();
    emit progressChanged();
}

GameImporter::ImportResult GameImporter::runImport(const QVariantList &items, bool move)
{
    ImportResult result;
    const auto cancel = m_cancel;
    qint64 lastReported = 0;
    auto report = [this, &lastReported](const QString &name, bool force) {
        const qint64 done = m_bytesDone.load();
        if (!force && done - lastReported < 16 * 1024 * 1024)
            return;
        lastReported = done;
        QMetaObject::invokeMethod(this, [this, name]() {
            m_currentName = name;
            emit progressChanged();
        }, Qt::QueuedConnection);
    };

    // Copies source to destination through a ".part" file, so an interrupted
    // copy never leaves a half-written game in the library.
    auto copyFile = [&](const QString &source, const QString &destination, const QString &name) {
        QFile in(source);
        const QString partial = destination + QStringLiteral(".part");
        QFile out(partial);
        if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        QByteArray buffer;
        while (!in.atEnd()) {
            if (cancel->load()) {
                out.close();
                QFile::remove(partial);
                return false;
            }
            buffer = in.read(CopyChunk);
            if (buffer.isEmpty() || out.write(buffer) != buffer.size()) {
                out.close();
                QFile::remove(partial);
                return false;
            }
            m_bytesDone += buffer.size();
            report(name, false);
        }
        out.close();
        if (!QFile::rename(partial, destination)) {
            QFile::remove(partial);
            return false;
        }
        return true;
    };

    auto extractRom = [&](const QString &archivePath, const QString &entryName,
                          const QString &destination) {
        archive *reader = archive_read_new();
        archive_read_support_filter_all(reader);
        archive_read_support_format_zip(reader);
        archive_read_support_format_7zip(reader);
        bool ok = false;
        if (archive_read_open_filename(reader, QFile::encodeName(archivePath).constData(), 64 * 1024) == ARCHIVE_OK) {
            archive_entry *entry = nullptr;
            while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) {
                const QString name = QFileInfo(QString::fromUtf8(archive_entry_pathname(entry))).fileName();
                if (archive_entry_filetype(entry) != AE_IFREG || name != entryName) {
                    archive_read_data_skip(reader);
                    continue;
                }
                const QString partial = destination + QStringLiteral(".part");
                QFile out(partial);
                if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    break;
                char buffer[256 * 1024];
                ok = true;
                for (;;) {
                    if (cancel->load()) { ok = false; break; }
                    const la_ssize_t count = archive_read_data(reader, buffer, sizeof(buffer));
                    if (count == 0)
                        break;
                    if (count < 0 || out.write(buffer, count) != count) { ok = false; break; }
                    m_bytesDone += count;
                    report(entryName, false);
                }
                out.close();
                if (ok)
                    ok = QFile::rename(partial, destination);
                if (!ok)
                    QFile::remove(partial);
                break;
            }
        }
        archive_read_free(reader);
        return ok;
    };

    for (const QVariant &entry : items) {
        if (cancel->load())
            break;
        const QVariantMap item = entry.toMap();
        const QString name = item.value(QStringLiteral("name")).toString();
        const QString destinationDirectory = item.value(QStringLiteral("destination")).toString();
        const QString archiveEntry = item.value(QStringLiteral("archiveEntry")).toString();
        const QStringList files = item.value(QStringLiteral("files")).toStringList();
        report(name, true);

        if (destinationDirectory.isEmpty() || !QDir().mkpath(destinationDirectory)) {
            ++result.failed;
            ++m_completed;
            qCWarning(lcLibrary) << "Import failed, no platform folder for" << name;
            continue;
        }

        // Never overwrite: if any destination file exists, skip the game.
        QStringList destinations;
        if (!archiveEntry.isEmpty())
            destinations.append(QDir(destinationDirectory).filePath(archiveEntry));
        else
            for (const QString &file : files)
                destinations.append(QDir(destinationDirectory).filePath(QFileInfo(file).fileName()));
        bool exists = false;
        for (const QString &destination : destinations)
            exists = exists || QFileInfo::exists(destination);
        if (exists) {
            ++result.skipped;
            ++m_completed;
            m_bytesDone += item.value(QStringLiteral("size")).toLongLong();
            qCInfo(lcLibrary) << "Import skipped, already in the library:" << name;
            continue;
        }

        bool ok = true;
        QStringList written;
        if (!archiveEntry.isEmpty()) {
            // Archives are unpacked; the archive itself is left where it was.
            ok = extractRom(files.value(0), archiveEntry, destinations.value(0));
            if (ok)
                written.append(destinations.value(0));
        } else {
            for (int i = 0; i < files.size() && ok; ++i) {
                const QString &source = files.at(i);
                const QString &destination = destinations.at(i);
                if (move && QFile::rename(source, destination)) {
                    m_bytesDone += QFileInfo(destination).size();
                    written.append(destination);
                    continue;
                }
                ok = copyFile(source, destination, name);
                if (ok)
                    written.append(destination);
            }
            // Moving across drives: remove the originals only once every
            // file of the game has been copied.
            if (ok && move) {
                for (const QString &source : files) {
                    if (QFileInfo::exists(source))
                        QFile::remove(source);
                }
            }
        }
        if (!ok) {
            // Roll back a partly imported multi-file game (copies only).
            if (!move)
                for (const QString &path : written)
                    QFile::remove(path);
            ++result.failed;
            qCWarning(lcLibrary) << "Import failed for" << name;
        } else {
            ++result.imported;
            result.importedPaths.append(item.value(QStringLiteral("path")).toString()
                                        + QLatin1Char('|') + archiveEntry);
            qCInfo(lcLibrary) << "Imported" << name << "into" << destinationDirectory
                              << (move ? "(moved)" : "(copied)");
        }
        ++m_completed;
        report(name, true);
    }
    return result;
}
