#include "RomInspector.h"

#include "metadata/N64RomHash.h"

#include <QFile>
#include <QFileInfo>

#define XXH_INLINE_ALL
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"

QString RomInspector::regionName(QChar regionCode)
{
    switch (regionCode.toLatin1()) {
    case 'E': return QStringLiteral("North America");
    case 'J': return QStringLiteral("Japan");
    case 'P': case 'X': case 'Y': case 'Z':
        return QStringLiteral("Europe");
    case 'D': return QStringLiteral("Germany");
    case 'F': return QStringLiteral("France");
    case 'I': return QStringLiteral("Italy");
    case 'S': return QStringLiteral("Spain");
    case 'U': return QStringLiteral("Australia");
    case 'A': return QStringLiteral("Asia");
    case 'C': return QStringLiteral("China");
    case 'K': return QStringLiteral("Korea");
    default: return QStringLiteral("an unknown region");
    }
}

QString N64RomInfo::regionName() const
{
    return RomInspector::regionName(regionCode);
}

QString N64RomInfo::versionLabel() const
{
    return regionName() + QStringLiteral(" 1.") + QString::number(revision);
}

QString N64RomInfo::xxh3Hex() const
{
    return QStringLiteral("%1").arg(xxh3, 16, 16, QLatin1Char('0')).toUpper();
}

QByteArray RomInspector::canonicalN64(const QByteArray &romData, QString *errorMessage)
{
    QByteArray padded = romData;
    padded.resize((padded.size() + 3) & ~qsizetype(3), '\0');
    QByteArray canonical;
    if (!N64RomHash::toBigEndian(padded, &canonical, errorMessage))
        return {};
    return canonical;
}

N64RomInfo RomInspector::inspectN64(const QByteArray &romData)
{
    N64RomInfo info;
    if (romData.size() < 0x1000) {
        info.error = QStringLiteral("The file is too small to be an N64 ROM.");
        return info;
    }

    const QByteArray canonical = canonicalN64(romData, &info.error);
    if (canonical.isEmpty())
        return info;

    QByteArray name = canonical.mid(0x20, 20);
    const int nul = name.indexOf('\0');
    if (nul >= 0)
        name.truncate(nul);
    info.internalName = QString::fromLatin1(name).trimmed();
    info.gameCode = QString::fromLatin1(canonical.mid(0x3B, 4));
    info.regionCode = QChar::fromLatin1(canonical.at(0x3E));
    info.revision = static_cast<unsigned char>(canonical.at(0x3F));
    info.xxh3 = XXH3_64bits(canonical.constData(), static_cast<size_t>(canonical.size()));
    info.valid = true;
    return info;
}

N64RomInfo RomInspector::inspectN64File(const QString &path)
{
    N64RomInfo info;
    const QFileInfo fileInfo(path);
    if (!fileInfo.isFile()) {
        info.error = QStringLiteral("The game file was not found.");
        return info;
    }
    if (fileInfo.size() > MaximumN64RomSize) {
        info.error = QStringLiteral("The file is too large to be an N64 ROM.");
        return info;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        info.error = QStringLiteral("The game file could not be read.");
        return info;
    }
    return inspectN64(file.readAll());
}
