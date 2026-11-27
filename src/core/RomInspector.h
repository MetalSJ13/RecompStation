#pragma once

#include <QByteArray>
#include <QString>

// Reads what a launcher needs to know about an N64 ROM: the title and region
// in its header, and the XXH3-64 checksum of its big-endian bytes (the value
// the N64Recomp launchers compare against before they accept a ROM).
struct N64RomInfo
{
    bool valid = false;
    QString internalName;   // header 0x20, 20 bytes, trailing spaces removed
    QString gameCode;       // header 0x3B, 4 characters, e.g. "NBKE"
    QChar regionCode;       // last character of the game code, e.g. 'E'
    int revision = 0;       // header 0x3F
    quint64 xxh3 = 0;       // of the padded big-endian ROM bytes
    QString sha1;           // of the same bytes, 40 lower-case hex digits
    QString error;

    QString regionName() const;          // "North America", "Europe", ...
    QString versionLabel() const;        // "North America 1.0"
    QString xxh3Hex() const;             // 16 upper-case hex digits
};

class RomInspector
{
public:
    // Largest file treated as an N64 ROM (the biggest retail cartridge is 64 MiB).
    static constexpr qint64 MaximumN64RomSize = 128ll * 1024 * 1024;

    static N64RomInfo inspectN64(const QByteArray &romData);
    static N64RomInfo inspectN64File(const QString &path);

    // Pads to a multiple of four and converts to big-endian (.z64) order, the
    // form the N64Recomp launchers store. Returns an empty array on failure.
    static QByteArray canonicalN64(const QByteArray &romData, QString *errorMessage = nullptr);

    static QString regionName(QChar regionCode);
};
