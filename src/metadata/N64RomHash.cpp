#include "N64RomHash.h"

#include <QCryptographicHash>

#include <utility>

namespace {
constexpr quint32 Crc32Polynomial = 0xEDB88320u;

QString crc32Hex(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (const unsigned char byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? Crc32Polynomial : 0u);
    }
    crc ^= 0xFFFFFFFFu;
    return QStringLiteral("%1").arg(crc, 8, 16, QLatin1Char('0'));
}
}

bool N64RomHash::toBigEndian(const QByteArray &romData,
                             QByteArray *canonicalData,
                             QString *errorMessage)
{
    if (!canonicalData) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A canonical ROM destination is required.");
        return false;
    }

    if (romData.size() < 4 || (romData.size() % 4) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The N64 ROM size must be at least four bytes and divisible by four.");
        return false;
    }

    const auto byteAt = [&romData](qsizetype index) {
        return static_cast<unsigned char>(romData.at(index));
    };

    const bool bigEndian =
        byteAt(0) == 0x80 && byteAt(1) == 0x37
        && byteAt(2) == 0x12 && byteAt(3) == 0x40;
    const bool byteSwapped =
        byteAt(0) == 0x37 && byteAt(1) == 0x80
        && byteAt(2) == 0x40 && byteAt(3) == 0x12;
    const bool littleEndian =
        byteAt(0) == 0x40 && byteAt(1) == 0x12
        && byteAt(2) == 0x37 && byteAt(3) == 0x80;

    if (!bigEndian && !byteSwapped && !littleEndian) {
        if (errorMessage)
            *errorMessage = QStringLiteral("The file does not have a recognized N64 ROM byte-order signature.");
        return false;
    }

    if (bigEndian) {
        *canonicalData = romData;
    } else {
        QByteArray normalized(romData.size(), '\0');
        for (qsizetype offset = 0; offset < romData.size(); offset += 4) {
            if (byteSwapped) {
                normalized[offset] = romData.at(offset + 1);
                normalized[offset + 1] = romData.at(offset);
                normalized[offset + 2] = romData.at(offset + 3);
                normalized[offset + 3] = romData.at(offset + 2);
            } else {
                normalized[offset] = romData.at(offset + 3);
                normalized[offset + 1] = romData.at(offset + 2);
                normalized[offset + 2] = romData.at(offset + 1);
                normalized[offset + 3] = romData.at(offset);
            }
        }
        *canonicalData = std::move(normalized);
    }

    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool N64RomHash::calculate(const QByteArray &romData,
                           N64RomHashes *hashes,
                           QString *errorMessage)
{
    if (!hashes) {
        if (errorMessage)
            *errorMessage = QStringLiteral("A hash destination is required.");
        return false;
    }

    QByteArray canonicalData;
    if (!toBigEndian(romData, &canonicalData, errorMessage))
        return false;

    hashes->md5 = QString::fromLatin1(
        QCryptographicHash::hash(canonicalData, QCryptographicHash::Md5).toHex());
    hashes->crc32 = crc32Hex(canonicalData);
    if (errorMessage)
        errorMessage->clear();
    return true;
}
