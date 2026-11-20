#pragma once

#include <QString>
#include <atomic>
#include <functional>

struct LaunchBoxExtractedMetadata
{
    QString platformsXmlPath;
    QString filesXmlPath;
    QString metadataXmlPath;
};

class LaunchBoxArchiveExtractor
{
public:
    struct Result
    {
        bool success = false;
        QString extractedDirectory;
        LaunchBoxExtractedMetadata files;
        QString error;
    };

    // Extracts only the three required LaunchBox XML catalogs. Archive paths
    // are never written directly; links, special entries, duplicate catalog
    // files, and oversized archives are rejected.
    static Result extractMetadataArchive(
        const QString &archivePath,
        const QString &destinationDirectory,
        const std::atomic_bool *cancelled = nullptr,
        const std::function<void(qint64, qint64)> &progress = {}
    );
};
