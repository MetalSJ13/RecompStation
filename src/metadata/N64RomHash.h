#pragma once

#include <QByteArray>
#include <QString>

struct N64RomHashes
{
    QString md5;
    QString crc32;
};

class N64RomHash
{
public:
    // Calculates hashes from the canonical big-endian N64 ROM byte stream.
    // The input byte order is detected from the ROM magic, not its extension.
    static bool calculate(const QByteArray &romData,
                          N64RomHashes *hashes,
                          QString *errorMessage = nullptr);

    static bool toBigEndian(const QByteArray &romData,
                            QByteArray *canonicalData,
                            QString *errorMessage = nullptr);
};
