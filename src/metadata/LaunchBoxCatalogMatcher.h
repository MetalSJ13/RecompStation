#pragma once

#include "LaunchBoxXmlParser.h"

#include <QString>
#include <QList>

struct LaunchBoxMatchResult
{
    enum class Status {
        Matched,
        NoMatch,
        Ambiguous,
        UnsupportedPlatform
    };

    Status status = Status::NoMatch;
    int index = -1;
    QString reason;

    bool isMatch() const { return status == Status::Matched && index >= 0; }
};

class LaunchBoxCatalogMatcher
{
public:
    // Only explicit canonical mappings are accepted. LaunchBox aliases are
    // reference data and are deliberately not used as interchangeable names.
    static QString canonicalPlatformForInternalName(const QString &internalName);

    // First resolve the local ROM filename through Files.xml. If that
    // association identifies a GameName, resolve it against Metadata.xml.
    static LaunchBoxMatchResult matchByFileAssociation(
        const QString &internalPlatform,
        const QString &localFileName,
        const QList<LaunchBoxFileAssociation> &associations,
        const QList<LaunchBoxGameRecord> &games);

    // Conservative fallback for cases where Files.xml has no usable mapping.
    static LaunchBoxMatchResult matchByTitle(
        const QString &internalPlatform,
        const QString &localTitle,
        const QList<LaunchBoxGameRecord> &games);

    static QString normalizedTitle(const QString &title);
};
